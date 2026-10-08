/*
 * Experiment 2: DHT11 temperature/humidity on a WeAct Black Pill (STM32F401CCU6, 84 MHz).
 *
 * DATA on PA6 (TIM3_CH1) with external pull-up. Every READ_PERIOD_MS the driver
 * sends the start pulse, captures the frame with TIM3 input capture (interrupt per
 * edge, no blocking) and decodes it with a threshold derived from the frame itself.
 *
 * Per read, one line on USART1:
 *   T=<x> C RH=<y> % dec=<T dec>,<RH dec> raw=<5 bytes hex> ok=<n> crc_err=<n>
 *   timeout=<n> thr=<us> w0=<min>-<max>us w1=<min>-<max>us
 * First successful read: protocol timing line + all edge widths as CSV.
 * Integer arithmetic only.
 */
#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "config.h"
#include "dht11.h"

#if HSE_VALUE != 25000000U
#error "WeAct Black Pill F401CC has a 25 MHz crystal: HSE_VALUE must be 25000000"
#endif

/* Live Watch */
volatile uint32_t dht_ok_count;
volatile uint32_t dht_crc_err_count;
volatile uint32_t dht_timeout_count;

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
    clk.APB1CLKDivider = RCC_HCLK_DIV2;     /* PCLK1 = 42 MHz (max), TIM3 clock = 84 MHz */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;     /* PCLK2 = 84 MHz */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

static void GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);       /* active-low: off */
    g.Pin = LED_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &g);

#if DEBUG_ENABLED
    g.Pin = UART_TX_PIN;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF7_USART1;
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

/* ---------------- UART text output (integers only, no printf) ---------------- */

static char tx_buf[160];
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

static void tx_hex8(uint8_t v)
{
    static const char hex[] = "0123456789ABCDEF";
    char s[3] = {hex[v >> 4], hex[v & 0x0FU], '\0'};

    tx_str(s);
}

static void tx_range(uint16_t mn, uint16_t mx)
{
    if (mn > mx) {          /* no width in this class */
        tx_str("-");
        return;
    }
    tx_u32(mn);
    tx_str("-");
    tx_u32(mx);
}

static void tx_line(void)
{
    tx_str("\r\n");
#if DEBUG_ENABLED
    HAL_UART_Transmit(&huart1, (uint8_t *)tx_buf, (uint16_t)tx_len, HAL_MAX_DELAY);
#endif
    tx_len = 0;
}

static void print_read(const dht11_result_t *r)
{
    uint32_t i;

    if (r->status == DHT11_OK) {
        tx_str("T=");
        tx_u32(r->raw[2]);
        tx_str(" C RH=");
        tx_u32(r->raw[0]);
        tx_str(" % dec=");
        tx_u32(r->raw[3]);
        tx_str(",");
        tx_u32(r->raw[1]);
    } else {
        tx_str("T=- C RH=- % dec=-,-");
    }

    tx_str(" raw=");
    if (r->bits_valid) {
        for (i = 0; i < 5U; i++) {
            if (i != 0U) {
                tx_str(",");
            }
            tx_hex8(r->raw[i]);
        }
    } else {
        tx_str("-");
    }

    tx_str(" ok=");
    tx_u32(dht_ok_count);
    tx_str(" crc_err=");
    tx_u32(dht_crc_err_count);
    tx_str(" timeout=");
    tx_u32(dht_timeout_count);

    tx_str(" thr=");
    if (r->bits_valid) {
        tx_u32(r->thr_us);
        tx_str(" w0=");
        tx_range(r->w0_min, r->w0_max);
        tx_str("us w1=");
        tx_range(r->w1_min, r->w1_max);
        tx_str("us");
    } else {
        tx_str("- w0=- w1=-");
    }
    tx_line();
}

/* Once, for the first successful read: measured protocol timing + every edge width. */
static void print_first_dump(const dht11_result_t *r)
{
    const uint32_t n = dht11_segment_count();
    uint32_t i;

    tx_str("# timing: start_low=");
    tx_u32(r->start_low_us);
    tx_str(" us release_to_response=");
    tx_u32(r->release_to_resp_us);
    tx_str(" us resp_low=");
    tx_u32(r->resp_low_us);
    tx_str(" us resp_high=");
    tx_u32(r->resp_high_us);
    tx_str(" us bit_low=");
    tx_range(r->lo_min, r->lo_max);
    tx_str(" us final_low=");
    tx_u32(r->final_low_us);
    tx_str(" us");
    tx_line();

    tx_str("# edge dump: edges=");
    tx_u32(r->edges);
    tx_str(" segments=");
    tx_u32(n);
    tx_str(" segment0_level=");
    tx_u32(n ? dht11_segment_level(0) : 0U);
    tx_str(" (levels alternate)");
    tx_line();
    tx_str("index,width_us");
    tx_line();
    for (i = 0; i < n; i++) {
        tx_u32(i);
        tx_str(",");
        tx_u32(dht11_segment_width(i));
        tx_line();
    }
}

int main(void)
{
    dht11_result_t res;
    uint8_t dumped = 0;

    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
#if DEBUG_ENABLED
    USART1_Init();
#endif
    dht11_init();

    tx_str("# exp02_dht11 SYSCLK=");
    tx_u32(HAL_RCC_GetSysClockFreq());
    tx_str(" Hz pullup=");
    tx_u32(PULLUP_OHM);
    tx_str(" ohm period=");
    tx_u32(READ_PERIOD_MS);
    tx_str(" ms start_low=");
    tx_u32(START_LOW_MS);
    tx_str(" ms first read after ");
    tx_u32(POWERUP_WAIT_MS);
    tx_str(" ms");
    tx_line();

    for (;;) {
        if (dht11_poll(&res)) {
            if (res.status == DHT11_OK) {
                dht_ok_count++;
                HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
            } else if (res.status == DHT11_CRC_ERR) {
                dht_crc_err_count++;
            } else {
                dht_timeout_count++;
            }
            print_read(&res);
            if (res.status == DHT11_OK && !dumped) {
                print_first_dump(&res);
                dumped = 1;
            }
        }
    }
}
