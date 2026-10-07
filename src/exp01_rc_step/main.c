/*
 * Experiment 1: RC step response on a WeAct Black Pill (STM32F401CCU6, 84 MHz).
 *
 * Every CYCLE_PERIOD_MS:
 *   a. PB0 LOW for DISCHARGE_MS (full discharge)
 *   b. PB0 HIGH, capture N_SAMPLES charge samples (TIM2 TRGO -> ADC1, exact spacing)
 *   c. hold HIGH, then PB0 LOW and capture the discharge curve the same way
 *   d. print both curves as CSV (phase,t_us,adc_counts,mv) — only after capture
 *   e. print summary: final voltage, measured vs expected tau, deviation
 *
 * Integer math only: voltages in mV, times in us, deviation in 1/100 percent.
 */
#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "config.h"

#if HSE_VALUE != 25000000U
#error "WeAct Black Pill F401CC has a 25 MHz crystal: HSE_VALUE must be 25000000"
#endif

/* Live Watch: measured charge tau (63.2 % point), updated every cycle. */
volatile uint32_t tau_measured_us;
volatile uint32_t tau_discharge_us;

static uint16_t charge_buf[N_SAMPLES];
static uint16_t discharge_buf[N_SAMPLES];

static ADC_HandleTypeDef hadc1;
static TIM_HandleTypeDef htim2;
#if DEBUG_ENABLED
static UART_HandleTypeDef huart1;
#endif

static void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    /* F401: Scale 2 supports up to 84 MHz */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 25;              /* 25 MHz / 25 = 1 MHz VCO input */
    osc.PLL.PLLN = 336;             /* VCO = 336 MHz */
    osc.PLL.PLLP = RCC_PLLP_DIV4;   /* SYSCLK = 84 MHz */
    osc.PLL.PLLQ = 7;               /* 48 MHz for USB OTG FS */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;    /* HCLK  = 84 MHz */
    clk.APB1CLKDivider = RCC_HCLK_DIV2;     /* PCLK1 = 42 MHz (max), TIM2 clock = 84 MHz */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;     /* PCLK2 = 84 MHz */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

static void GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);       /* active-low: off */
    HAL_GPIO_WritePin(STEP_PORT, STEP_PIN, GPIO_PIN_RESET);

    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin = LED_PIN;
    HAL_GPIO_Init(LED_PORT, &g);
    g.Pin = STEP_PIN;
    HAL_GPIO_Init(STEP_PORT, &g);

    g.Mode = GPIO_MODE_ANALOG;
    g.Pin = SENSE_PIN;
    HAL_GPIO_Init(SENSE_PORT, &g);

#if DEBUG_ENABLED
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF7_USART1;
    g.Pin = UART_TX_PIN;
    HAL_GPIO_Init(UART_TX_PORT, &g);
#endif
}

#if DEBUG_ENABLED
static void USART1_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}
#endif

static void ADC1_Init(void)
{
    ADC_ChannelConfTypeDef ch = {0};

    __HAL_RCC_ADC1_CLK_ENABLE();
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;   /* 84 / 4 = 21 MHz (max 36) */
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_TRGO;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        Error_Handler();
    }

    /* Longest sample time: source impedance is up to R (~10 kOhm).
     * (480 + 12) cycles / 21 MHz = 23.4 us per conversion, well inside 1 ms. */
    ch.Channel = SENSE_CHANNEL;
    ch.Rank = 1;
    ch.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) {
        Error_Handler();
    }
}

static void TIM2_Init(void)
{
    TIM_MasterConfigTypeDef m = {0};

    __HAL_RCC_TIM2_CLK_ENABLE();
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 84 - 1;                  /* 84 MHz / 84 = 1 MHz tick */
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = SAMPLE_PERIOD_US - 1;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
        Error_Handler();
    }

    m.MasterOutputTrigger = TIM_TRGO_UPDATE;        /* every update event triggers one conversion */
    m.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &m) != HAL_OK) {
        Error_Handler();
    }
}

/* Drive PB0 to `level` and capture N_SAMPLES conversions, sample k at t = k * SAMPLE_PERIOD_US. */
static void capture(uint16_t *buf, GPIO_PinState level)
{
    uint32_t i;

    if (HAL_ADC_Start(&hadc1) != HAL_OK) {          /* ADON, armed for TIM2 TRGO */
        Error_Handler();
    }

    HAL_GPIO_WritePin(STEP_PORT, STEP_PIN, level);  /* the step: t = 0 */
    TIM2->CR1 |= TIM_CR1_CEN;
    TIM2->EGR = TIM_EGR_UG;                         /* counter = 0 + immediate TRGO: sample 0 at t = 0 */

    for (i = 0; i < N_SAMPLES; i++) {
        if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) {
            Error_Handler();
        }
        buf[i] = (uint16_t)HAL_ADC_GetValue(&hadc1);
        if ((i % BLINK_EVERY_N) == 0) {
            HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        }
    }

    TIM2->CR1 &= ~TIM_CR1_CEN;
    HAL_ADC_Stop(&hadc1);
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
}

static uint32_t counts_to_mv(uint32_t counts)
{
    return (counts * VDDA_MV + ADC_FULL_SCALE / 2) / ADC_FULL_SCALE;
}

/* Mean of the last SETTLE_AVG_N samples, in counts. */
static uint32_t final_counts(const uint16_t *buf)
{
    uint32_t sum = 0;
    uint32_t i;

    for (i = N_SAMPLES - SETTLE_AVG_N; i < N_SAMPLES; i++) {
        sum += buf[i];
    }
    return (sum + SETTLE_AVG_N / 2) / SETTLE_AVG_N;
}

/*
 * Time (us) at which the curve has covered TAU_STEP_PERMILLE of the step from
 * v0 to vf (both in counts), linearly interpolated between samples.
 * Rising (charge) -> 63.2 % point; falling (discharge) -> 36.8 % point.
 * Returns 0 if the threshold is never crossed.
 */
static uint32_t tau_from_curve(const uint16_t *buf, int32_t v0, int32_t vf)
{
    const int32_t thr = v0 * 1000 + TAU_STEP_PERMILLE * (vf - v0);   /* counts x1000 */
    const int32_t dir = (vf >= v0) ? 1 : -1;
    int32_t prev = ((int32_t)buf[0] * 1000 - thr) * dir;
    uint32_t i;

    if (vf == v0 || prev >= 0) {
        return 0;
    }
    for (i = 1; i < N_SAMPLES; i++) {
        int32_t d = ((int32_t)buf[i] * 1000 - thr) * dir;
        if (d >= 0) {
            /* prev < 0 <= d */
            uint32_t frac_us = (uint32_t)(((uint64_t)(uint32_t)(-prev) * SAMPLE_PERIOD_US) /
                                          (uint32_t)(d - prev));
            return (i - 1) * SAMPLE_PERIOD_US + frac_us;
        }
        prev = d;
    }
    return 0;
}

/* Deviation of measured from expected tau, in 1/100 percent. */
static int32_t deviation_x100(uint32_t measured_us)
{
    int64_t diff = (int64_t)measured_us - (int64_t)TAU_EXPECTED_US;
    return (int32_t)((diff * 10000) / (int64_t)TAU_EXPECTED_US);
}

/* ---------------- UART text output (integers only, no printf) ---------------- */

static char tx_buf[96];
static uint32_t tx_len;

static void tx_str(const char *s)
{
    while (*s != '\0' && tx_len < sizeof(tx_buf)) {
        tx_buf[tx_len++] = *s++;
    }
}

static void tx_u32(uint32_t v)
{
    char d[10];
    uint32_t n = 0;

    do {
        d[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v != 0);
    while (n > 0 && tx_len < sizeof(tx_buf)) {
        tx_buf[tx_len++] = d[--n];
    }
}

/* Signed fixed-point with two decimals, e.g. -31 -> "-0.31", 142 -> "+1.42". */
static void tx_fix2(int32_t x100)
{
    uint32_t a = (x100 < 0) ? (uint32_t)(-x100) : (uint32_t)x100;
    char frac[4] = {'.', 0, 0, '\0'};

    tx_str((x100 < 0) ? "-" : "+");
    tx_u32(a / 100);
    frac[1] = (char)('0' + (a % 100) / 10);
    frac[2] = (char)('0' + a % 10);
    tx_str(frac);
}

static void tx_line(void)
{
    tx_str("\r\n");
#if DEBUG_ENABLED
    HAL_UART_Transmit(&huart1, (uint8_t *)tx_buf, (uint16_t)tx_len, HAL_MAX_DELAY);
#endif
    tx_len = 0;
}

static void print_curve(const char *phase, const uint16_t *buf)
{
    uint32_t i;

    for (i = 0; i < N_SAMPLES; i++) {
        tx_str(phase);
        tx_str(",");
        tx_u32(i * SAMPLE_PERIOD_US);
        tx_str(",");
        tx_u32(buf[i]);
        tx_str(",");
        tx_u32(counts_to_mv(buf[i]));
        tx_line();
    }
}

static void print_summary(const char *phase, uint32_t v0, uint32_t vf, uint32_t tau_us)
{
    tx_str("# ");
    tx_str(phase);
    tx_str(": v0=");
    tx_u32(counts_to_mv(v0));
    tx_str(" mV final=");
    tx_u32(counts_to_mv(vf));
    tx_str(" mV tau=");
    if (tau_us == 0) {
        tx_str("not_found");
        tx_line();
        return;
    }
    tx_u32(tau_us);
    tx_str(" us expected=");
    tx_u32(TAU_EXPECTED_US);
    tx_str(" us dev=");
    tx_fix2(deviation_x100(tau_us));
    tx_str(" %");
    tx_line();
}

int main(void)
{
    uint32_t cycle = 0;

    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
#if DEBUG_ENABLED
    USART1_Init();
#endif
    ADC1_Init();
    TIM2_Init();

    tx_str("# exp01_rc_step SYSCLK=");
    tx_u32(HAL_RCC_GetSysClockFreq());
    tx_str(" Hz R=");
    tx_u32(R_NOMINAL_OHM);
    tx_str(" ohm C=");
    tx_u32(C_NOMINAL_NF);
    tx_str(" nF VDDA=");
    tx_u32(VDDA_MV);
    tx_str(" mV tau_expected=");
    tx_u32(TAU_EXPECTED_US);
    tx_str(" us");
    tx_line();

    for (;;) {
        const uint32_t t_start = HAL_GetTick();
        uint32_t c0, cf, d0, df;

        cycle++;

        /* a. full discharge */
        HAL_GPIO_WritePin(STEP_PORT, STEP_PIN, GPIO_PIN_RESET);
        HAL_Delay(DISCHARGE_MS);

        /* b. charge curve */
        capture(charge_buf, GPIO_PIN_SET);

        /* c. hold HIGH, then discharge curve */
        HAL_Delay(HOLD_HIGH_MS);
        capture(discharge_buf, GPIO_PIN_RESET);

        c0 = charge_buf[0];
        cf = final_counts(charge_buf);
        d0 = discharge_buf[0];
        df = final_counts(discharge_buf);
        tau_measured_us = tau_from_curve(charge_buf, (int32_t)c0, (int32_t)cf);
        tau_discharge_us = tau_from_curve(discharge_buf, (int32_t)d0, (int32_t)df);

        /* d. curves, only after capture is finished */
        tx_str("# cycle ");
        tx_u32(cycle);
        tx_line();
        tx_str("phase,t_us,adc_counts,mv");
        tx_line();
        print_curve("charge", charge_buf);
        print_curve("discharge", discharge_buf);

        /* e. summary */
        print_summary("charge", c0, cf, tau_measured_us);
        print_summary("discharge", d0, df, tau_discharge_us);

        while (HAL_GetTick() - t_start < CYCLE_PERIOD_MS) {
        }
    }
}
