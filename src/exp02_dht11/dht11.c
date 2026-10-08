/*
 * DHT11 single-wire driver: start pulse via open-drain GPIO, frame capture with
 * TIM3_CH1 input capture on both edges (1 us/tick), decode after the frame.
 *
 * Pin: PA6 is configured once as AF2 (TIM3_CH1) open-drain, no internal pull.
 * Only MODER is switched afterwards:
 *   output (ODR = 0, open-drain) -> N-MOS on, line LOW         (RM p.154)
 *   alternate function           -> TIM3_CH1 is an input, line released to
 *                                   the external pull-up
 *
 * Counter range: TIM3 is 16-bit (DS p.26, Table 4) -> wraps every 65,536 us.
 * Widths are taken as uint16 differences, which stay correct for any interval
 * below 65,536 us. Longest in-frame interval is ~80 us, the whole frame ~5 ms,
 * and the measured start pulse ~20 ms: all inside one wrap.
 */
#include <string.h>
#include "stm32f4xx_hal.h"
#include "config.h"
#include "dht11.h"

typedef enum {
    ST_IDLE,
    ST_START_LOW,
    ST_CAPTURE
} dht_state_t;

static TIM_HandleTypeDef htim3;

/* Written by the ISR while capture is armed; read only after disarm. */
static volatile uint16_t s_t[EDGE_BUF_LEN];
static volatile uint8_t  s_lvl[EDGE_BUF_LEN];
static volatile uint32_t s_n;
static volatile uint8_t  s_overflow;
static volatile uint8_t  s_overcapture;

static dht_state_t s_state = ST_IDLE;
static uint32_t s_next_start_ms = POWERUP_WAIT_MS;     /* HAL tick starts after power-up */
static uint32_t s_phase_ms;
static uint16_t s_cnt_low;
static uint16_t s_cnt_release;

/* Live Watch: reason of the last failed read (dht11_fail_t). */
volatile uint8_t dht_last_fail;

static void pin_mode_output(void)
{
    DHT_PORT->MODER = (DHT_PORT->MODER & ~(3U << (2U * DHT_PIN_POS))) | (1U << (2U * DHT_PIN_POS));
}

static void pin_mode_af(void)
{
    DHT_PORT->MODER = (DHT_PORT->MODER & ~(3U << (2U * DHT_PIN_POS))) | (2U << (2U * DHT_PIN_POS));
}

void dht11_init(void)
{
    GPIO_InitTypeDef g = {0};
    TIM_IC_InitTypeDef ic = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_RESET);  /* ODR = 0 for the start pulse */
    g.Pin = DHT_PIN;
    g.Mode = GPIO_MODE_AF_OD;       /* sets AFR = AF2 and OTYPER = open-drain; line released */
    g.Pull = GPIO_NOPULL;           /* external pull-up */
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Alternate = DHT_PIN_AF;
    HAL_GPIO_Init(DHT_PORT, &g);

    /* TIM3 clock = 2 x PCLK1 = 84 MHz (RM p.95, APB1 /2) -> /84 = 1 MHz */
    __HAL_RCC_TIM3_CLK_ENABLE();
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = (2U * HAL_RCC_GetPCLK1Freq()) / TIM_TICK_HZ - 1U;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 0xFFFFU;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_IC_Init(&htim3) != HAL_OK) {
        while (1) {
        }
    }

    ic.ICPolarity = TIM_INPUTCHANNELPOLARITY_BOTHEDGE;  /* CC1NP/CC1P = 11 (RM p.367) */
    ic.ICSelection = TIM_ICSELECTION_DIRECTTI;          /* CC1S = 01: IC1 on TI1 */
    ic.ICPrescaler = TIM_ICPSC_DIV1;
    ic.ICFilter = TIM_IC_FILTER;
    if (HAL_TIM_IC_ConfigChannel(&htim3, &ic, TIM_CHANNEL_1) != HAL_OK) {
        while (1) {
        }
    }

    HAL_NVIC_SetPriority(TIM3_IRQn, 0, 0);     /* above SysTick: level read right after the edge */
    HAL_NVIC_EnableIRQ(TIM3_IRQn);

    TIM3->CCER &= ~TIM_CCER_CC1E;               /* armed per read only */
    TIM3->CR1 |= TIM_CR1_CEN;                   /* free-running counter */
}

void dht11_capture_isr(void)
{
    if (TIM3->SR & TIM_SR_CC1IF) {
        /* read the data before the overcapture flag (RM p.333); reading CCR1 clears CC1IF */
        const uint16_t t = (uint16_t)TIM3->CCR1;
        const uint8_t lvl = (DHT_PORT->IDR & DHT_PIN) ? 1U : 0U;   /* level after this edge */

        if (TIM3->SR & TIM_SR_CC1OF) {
            TIM3->SR = ~TIM_SR_CC1OF;
            s_overcapture = 1;
        }
        if (s_n < EDGE_BUF_LEN) {
            s_t[s_n] = t;
            s_lvl[s_n] = lvl;
            s_n++;
        } else {
            s_overflow = 1;
        }
    }
}

static void capture_arm(void)
{
    s_n = 0;
    s_overflow = 0;
    s_overcapture = 0;
    TIM3->SR = ~(TIM_SR_CC1IF | TIM_SR_CC1OF);
    TIM3->DIER |= TIM_DIER_CC1IE;
    TIM3->CCER |= TIM_CCER_CC1E;
}

static void capture_disarm(void)
{
    TIM3->CCER &= ~TIM_CCER_CC1E;
    TIM3->DIER &= ~TIM_DIER_CC1IE;
}

uint32_t dht11_segment_count(void)
{
    return (s_n > 1U) ? s_n - 1U : 0U;
}

uint16_t dht11_segment_width(uint32_t i)
{
    return (uint16_t)(s_t[i + 1U] - s_t[i]);
}

uint8_t dht11_segment_level(uint32_t i)
{
    return s_lvl[i];
}

static int in_resp_window(uint16_t w)
{
    return w >= RESP_MIN_US && w <= RESP_MAX_US;
}

/*
 * Threshold between the "0" and "1" high widths, derived from this frame:
 * iterative two-means (midpoint of the two cluster means until stable).
 * If all 40 high widths sit in one cluster (spread < MIN_CLUSTER_GAP_US), the
 * measured mean bit-start low (~50 us, DHT p.7) is used: 0 = 26-28 us is
 * below it and 1 = 70 us above it (DHT Fig. 4/5).
 */
static uint16_t derive_threshold(const uint16_t *hw, uint16_t lo_mean)
{
    uint16_t mn = 0xFFFFU, mx = 0;
    uint32_t thr, k, it;

    for (k = 0; k < 40U; k++) {
        if (hw[k] < mn) {
            mn = hw[k];
        }
        if (hw[k] > mx) {
            mx = hw[k];
        }
    }
    if ((uint32_t)(mx - mn) < MIN_CLUSTER_GAP_US) {
        return lo_mean;
    }

    thr = ((uint32_t)mn + mx + 1U) / 2U;
    for (it = 0; it < 16U; it++) {
        uint32_t s0 = 0, n0 = 0, s1 = 0, n1 = 0, nt;

        for (k = 0; k < 40U; k++) {
            if (hw[k] >= thr) {
                s1 += hw[k];
                n1++;
            } else {
                s0 += hw[k];
                n0++;
            }
        }
        if (n0 == 0U || n1 == 0U) {
            break;
        }
        /* (s0/n0 + s1/n1) / 2, rounded */
        nt = (s0 * n1 + s1 * n0 + n0 * n1) / (2U * n0 * n1);
        if (nt == thr) {
            break;
        }
        thr = nt;
    }
    return (uint16_t)thr;
}

static void decode(dht11_result_t *r)
{
    const uint32_t n = s_n;
    uint16_t hw[40];
    uint32_t e, k, lo_sum = 0;
    uint8_t found = 0;

    memset(r, 0, sizeof(*r));
    r->status = DHT11_TIMEOUT;
    r->edges = n;
    r->start_low_us = (uint16_t)(s_cnt_release - s_cnt_low);
    r->lo_min = r->w0_min = r->w1_min = 0xFFFFU;

    if (s_overflow || s_overcapture) {
        r->fail = DHT11_FAIL_CAPTURE;
        return;
    }

    /* response: segment e low, e+1 high, both ~80 us (DHT p.7). Segment j = edge j -> j+1. */
    for (e = 0; e + 2U < n; e++) {
        if (s_lvl[e] == 0U && s_lvl[e + 1U] == 1U &&
            in_resp_window(dht11_segment_width(e)) && in_resp_window(dht11_segment_width(e + 1U))) {
            found = 1;
            break;
        }
    }
    if (!found) {
        r->fail = DHT11_FAIL_NO_RESPONSE;
        return;
    }
    r->release_to_resp_us = (uint16_t)(s_t[e] - s_cnt_release);
    r->resp_low_us = dht11_segment_width(e);
    r->resp_high_us = dht11_segment_width(e + 1U);

    /* bit k: low = segment e+2+2k, high = e+3+2k; bit 39 high ends at edge e+82 */
    if (e + 82U >= n) {
        r->fail = DHT11_FAIL_SHORT_FRAME;
        return;
    }
    for (k = 0; k < 40U; k++) {
        const uint32_t lo = e + 2U + 2U * k;
        const uint16_t lw = dht11_segment_width(lo);

        if (s_lvl[lo] != 0U || s_lvl[lo + 1U] != 1U) {
            r->fail = DHT11_FAIL_LEVELS;
            return;
        }
        hw[k] = dht11_segment_width(lo + 1U);
        lo_sum += lw;
        if (lw < r->lo_min) {
            r->lo_min = lw;
        }
        if (lw > r->lo_max) {
            r->lo_max = lw;
        }
    }
    if (e + 83U < n) {
        r->final_low_us = dht11_segment_width(e + 82U);
    }

    r->thr_us = derive_threshold(hw, (uint16_t)((lo_sum + 20U) / 40U));
    for (k = 0; k < 40U; k++) {
        const uint8_t bit = (hw[k] >= r->thr_us) ? 1U : 0U;

        r->raw[k / 8U] = (uint8_t)((r->raw[k / 8U] << 1) | bit);   /* MSB first (DHT p.5) */
        if (bit) {
            if (hw[k] < r->w1_min) {
                r->w1_min = hw[k];
            }
            if (hw[k] > r->w1_max) {
                r->w1_max = hw[k];
            }
        } else {
            if (hw[k] < r->w0_min) {
                r->w0_min = hw[k];
            }
            if (hw[k] > r->w0_max) {
                r->w0_max = hw[k];
            }
        }
    }
    r->bits_valid = 1;

    /* checksum = low 8 bits of the sum of the first four bytes (DHT p.5) */
    if ((uint8_t)(r->raw[0] + r->raw[1] + r->raw[2] + r->raw[3]) == r->raw[4]) {
        r->status = DHT11_OK;
    } else {
        r->status = DHT11_CRC_ERR;
    }
}

int dht11_poll(dht11_result_t *res)
{
    const uint32_t now = HAL_GetTick();

    switch (s_state) {
    case ST_IDLE:
        if ((int32_t)(now - s_next_start_ms) >= 0) {
            pin_mode_output();                  /* start signal: line LOW (DHT p.6) */
            s_cnt_low = (uint16_t)TIM3->CNT;
            s_phase_ms = now;
            s_next_start_ms = now + READ_PERIOD_MS;
            s_state = ST_START_LOW;
        }
        return 0;

    case ST_START_LOW:
        if (now - s_phase_ms >= START_LOW_MS) {
            capture_arm();
            pin_mode_af();                      /* release: pull-up takes the line HIGH */
            s_cnt_release = (uint16_t)TIM3->CNT;
            s_phase_ms = now;
            s_state = ST_CAPTURE;
        }
        return 0;

    case ST_CAPTURE:
    default:
        if (now - s_phase_ms >= CAPTURE_WINDOW_MS) {
            capture_disarm();
            decode(res);
            dht_last_fail = (uint8_t)res->fail;
            s_state = ST_IDLE;
            return 1;
        }
        return 0;
    }
}
