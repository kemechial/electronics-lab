/*
 * Experiment 4, Stage 1: I2C2 bus check and address scan (0x08-0x77), Blue Pill (STM32F103C8 clone).
 * Result lines on USART1 (PA9, 115200 8N1). Expected SSD1306 address 0x3C or 0x3D
 * (0111 10 SA0, SSD1306 p.19).
 * Clock: HSE 8 MHz x 9 = 72 MHz SYSCLK, APB1 /2 = 36 MHz = PCLK1 for I2C2.
 * I2C 100 kHz Sm: CCR = 36 MHz / (2 x 100 kHz) = 180 (RM0008 p.781-782),
 *                 TRISE = 1000 ns / 27.8 ns + 1 = 37 (RM0008 p.782); HAL computes both.
 * BUSY lock: ES096 Rev 15 section 2.8.7 p.20 (analog filter locks BUSY; SWRST alone does not clear it).
 * The documented 14-step unlock runs before every I2C2 init and after every BUSY/TIMEOUT; each run is logged.
 */
#include <stdint.h>
#include "stm32f1xx_hal.h"
#include "config.h"

#if HSE_VALUE != 8000000U
#error "Blue Pill has an 8 MHz crystal: HSE_VALUE must be 8000000"
#endif

static I2C_HandleTypeDef hi2c2;
static UART_HandleTypeDef huart1;
static uint32_t recoveries;

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

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;              /* 8 MHz x 9 = 72 MHz */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;        /* 72 MHz */
    clk.APB1CLKDivider = RCC_HCLK_DIV2;         /* PCLK1 = 36 MHz (I2C2) */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;         /* 72 MHz */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
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

static void tx_hex8(uint8_t v)
{
    static const char hex[] = "0123456789ABCDEF";
    char s[5] = {'0', 'x', hex[v >> 4], hex[v & 0x0FU], '\0'};

    tx_str(s);
}

static void tx_line(void)
{
    tx_str("\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t *)tx_buf, (uint16_t)tx_len, HAL_MAX_DELAY);
    tx_len = 0;
}

static void UART_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();
    g.Pin = UART_TX_PIN;
    g.Mode = GPIO_MODE_AF_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(UART_TX_PORT, &g);

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

/* About 5 us at 72 MHz: half an SCL period at 100 kHz, used only during recovery. */
static void half_bit_wait(void)
{
    volatile uint32_t n = 60U;

    while (n-- > 0U) {
    }
}

/* Pins as GPIO inputs, no internal pull: only the external pull-ups count. */
static void read_lines(uint8_t *scl, uint8_t *sda)
{
    GPIO_InitTypeDef g = {0};

    g.Pin = I2C_SCL_PIN | I2C_SDA_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(I2C_PORT, &g);
    half_bit_wait();
    *scl = (uint8_t)HAL_GPIO_ReadPin(I2C_PORT, I2C_SCL_PIN);
    *sda = (uint8_t)HAL_GPIO_ReadPin(I2C_PORT, I2C_SDA_PIN);
}

static void report_lines(const char *when)
{
    uint8_t scl, sda;

    read_lines(&scl, &sda);
    tx_str("# ");
    tx_str(when);
    tx_str(": SCL=");
    tx_u32(scl);
    tx_str(" SDA=");
    tx_u32(sda);
    tx_str(scl && sda ? " OK (both high)" : " STUCK LOW: missing pull-ups or wrong wiring");
    tx_line();
}

static uint32_t errata_unlock(void);

static void I2C2_Init(void)
{
    const uint32_t failed = errata_unlock();    /* ES096 2.8.7, ends with AF open-drain + SWRST */

    if (failed) {
        tx_str("# ES096 2.8.7 unlock before init: check failed at step ");
        tx_u32(failed);
        tx_line();
    }
    hi2c2.Instance = I2C2;
    hi2c2.Init.ClockSpeed = I2C_SPEED_HZ;
    hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c2.Init.OwnAddress1 = 0;
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
        tx_str("# I2C2 init FAILED");
        tx_line();
        return;
    }
    tx_str("# I2C2 ");
    tx_u32(I2C_SPEED_HZ);
    tx_str(" Hz: CCR=");
    tx_u32(I2C2->CCR & I2C_CCR_CCR);
    tx_str(" TRISE=");
    tx_u32(I2C2->TRISE);
    tx_line();
}

/* Drive one line as open-drain GPIO to `level`, then check it in IDR (ES096 2.8.7 steps 2-11). */
static int drive_check(uint16_t pin, GPIO_PinState level)
{
    uint32_t n;

    HAL_GPIO_WritePin(I2C_PORT, pin, level);
    for (n = 0; n < 1000U; n++) {
        if (HAL_GPIO_ReadPin(I2C_PORT, pin) == level) {
            return 1;
        }
    }
    return 0;
}

/* ES096 Rev 15 section 2.8.7 p.20 workaround, steps 1-14. Returns the failed step, or 0. */
static uint32_t errata_unlock(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_I2C2_CLK_ENABLE();
    I2C2->CR1 &= ~I2C_CR1_PE;                                   /* 1 */
    HAL_GPIO_WritePin(I2C_PORT, I2C_SCL_PIN | I2C_SDA_PIN, GPIO_PIN_SET);
    g.Pin = I2C_SCL_PIN | I2C_SDA_PIN;
    g.Mode = GPIO_MODE_OUTPUT_OD;                               /* 2 */
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(I2C_PORT, &g);
    if (!drive_check(I2C_SCL_PIN, GPIO_PIN_SET) || !drive_check(I2C_SDA_PIN, GPIO_PIN_SET)) {
        return 3;                                               /* 3 */
    }
    if (!drive_check(I2C_SDA_PIN, GPIO_PIN_RESET)) {            /* 4 */
        return 5;                                               /* 5 */
    }
    if (!drive_check(I2C_SCL_PIN, GPIO_PIN_RESET)) {            /* 6 */
        return 7;                                               /* 7 */
    }
    if (!drive_check(I2C_SCL_PIN, GPIO_PIN_SET)) {              /* 8 */
        return 9;                                               /* 9 */
    }
    if (!drive_check(I2C_SDA_PIN, GPIO_PIN_SET)) {              /* 10 */
        return 11;                                              /* 11 */
    }
    g.Mode = GPIO_MODE_AF_OD;                                   /* 12 */
    HAL_GPIO_Init(I2C_PORT, &g);
    I2C2->CR1 |= I2C_CR1_SWRST;                                 /* 13 */
    I2C2->CR1 &= ~I2C_CR1_SWRST;                                /* 14 */
    return 0;
}

/* Logged recovery: ES096 2.8.7 unlock, then HAL re-init. */
static void bus_recover(const char *reason)
{
    uint32_t failed;

    recoveries++;
    tx_str("# RECOVERY ");
    tx_u32(recoveries);
    tx_str(": ");
    tx_str(reason);
    tx_line();
    HAL_I2C_DeInit(&hi2c2);
    failed = errata_unlock();
    tx_str("# ES096 2.8.7 unlock: ");
    if (failed) {
        tx_str("check failed at step ");
        tx_u32(failed);
    } else {
        tx_str("OK");
    }
    tx_line();
    I2C2_Init();
}

static void scan(uint32_t n_scan)
{
    uint32_t addr, found = 0;

    tx_str("# scan ");
    tx_u32(n_scan);
    tx_str(" (recoveries so far ");
    tx_u32(recoveries);
    tx_str(")");
    tx_line();
    for (addr = I2C_SCAN_FIRST; addr <= I2C_SCAN_LAST; addr++) {
        const HAL_StatusTypeDef st = HAL_I2C_IsDeviceReady(&hi2c2, (uint16_t)(addr << 1), 1, I2C_PROBE_TIMEOUT_MS);

        if (st == HAL_OK) {
            tx_str("ACK ");
            tx_hex8((uint8_t)addr);
            tx_line();
            found++;
        } else if (st == HAL_BUSY || st == HAL_TIMEOUT) {
            tx_str("BUS ");
            tx_str(st == HAL_BUSY ? "BUSY" : "TIMEOUT");
            tx_str(" at ");
            tx_hex8((uint8_t)addr);
            tx_str(" err=");
            tx_u32(HAL_I2C_GetError(&hi2c2));
            tx_line();
            bus_recover(st == HAL_BUSY ? "BUSY during scan" : "TIMEOUT during scan");
            return;
        }
    }
    tx_str("found=");
    tx_u32(found);
    tx_line();
}

/* ---------------- Software I2C (bit-banged, open-drain GPIO) ---------------- */

static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void wait_us(uint32_t us)
{
    const uint32_t start = DWT->CYCCNT;
    const uint32_t cycles = us * (SystemCoreClock / 1000000U);

    while (DWT->CYCCNT - start < cycles) {
    }
}

static void sda_set(int high)
{
    HAL_GPIO_WritePin(I2C_PORT, I2C_SDA_PIN, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* Release SCL and wait until it is really high (slave may stretch). Returns 0 on timeout. */
static int scl_release(void)
{
    uint32_t n;

    HAL_GPIO_WritePin(I2C_PORT, I2C_SCL_PIN, GPIO_PIN_SET);
    for (n = 0; n < SW_I2C_STRETCH_US; n++) {
        if (HAL_GPIO_ReadPin(I2C_PORT, I2C_SCL_PIN) == GPIO_PIN_SET) {
            return 1;
        }
        wait_us(1);
    }
    return 0;
}

static void scl_low(void)
{
    HAL_GPIO_WritePin(I2C_PORT, I2C_SCL_PIN, GPIO_PIN_RESET);
}

static void sw_i2c_init(void)
{
    GPIO_InitTypeDef g = {0};

    HAL_GPIO_WritePin(I2C_PORT, I2C_SCL_PIN | I2C_SDA_PIN, GPIO_PIN_SET);
    g.Pin = I2C_SCL_PIN | I2C_SDA_PIN;
    g.Mode = GPIO_MODE_OUTPUT_OD;               /* external pull-ups take the lines high */
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(I2C_PORT, &g);
    dwt_init();
}

static void sw_start(void)
{
    sda_set(1);
    (void)scl_release();
    wait_us(SW_I2C_HALF_US);
    sda_set(0);                                 /* SDA falls while SCL high = START */
    wait_us(SW_I2C_HALF_US);
    scl_low();
}

static void sw_stop(void)
{
    sda_set(0);
    wait_us(SW_I2C_HALF_US);
    (void)scl_release();
    wait_us(SW_I2C_HALF_US);
    sda_set(1);                                 /* SDA rises while SCL high = STOP */
    wait_us(SW_I2C_HALF_US);
}

/* Send one byte MSB first; returns 1 if the slave ACKed (SDA low on the 9th clock). */
static int sw_write_byte(uint8_t b)
{
    uint32_t i;
    int ack;

    for (i = 0; i < 8U; i++) {
        sda_set((b & 0x80U) != 0U);
        b <<= 1;
        wait_us(SW_I2C_HALF_US);
        (void)scl_release();
        wait_us(SW_I2C_HALF_US);
        scl_low();
    }
    sda_set(1);                                 /* release SDA for the ACK bit */
    wait_us(SW_I2C_HALF_US);
    (void)scl_release();
    ack = (HAL_GPIO_ReadPin(I2C_PORT, I2C_SDA_PIN) == GPIO_PIN_RESET);
    wait_us(SW_I2C_HALF_US);
    scl_low();
    return ack;
}

static void scan_sw(uint32_t n_scan)
{
    uint32_t addr, found = 0;

    tx_str("# sw scan ");
    tx_u32(n_scan);
    tx_line();
    for (addr = I2C_SCAN_FIRST; addr <= I2C_SCAN_LAST; addr++) {
        int ack;

        sw_start();
        ack = sw_write_byte((uint8_t)(addr << 1));
        sw_stop();
        if (ack) {
            tx_str("ACK ");
            tx_hex8((uint8_t)addr);
            tx_line();
            found++;
        }
    }
    tx_str("found=");
    tx_u32(found);
    tx_line();
}

/* ---------------- SSD1306 over software I2C ---------------- */

static uint8_t fb[OLED_W * OLED_H / 8];        /* 1024 bytes, page-major: fb[page * 128 + x], bit = y % 8 */
static uint32_t oled_nacks;

/* One I2C write: address, control byte (0x00 commands, 0x40 data), payload. */
static void oled_write(uint8_t control, const uint8_t *buf, uint32_t n)
{
    uint32_t i;
    int ok;

    sw_start();
    ok = sw_write_byte((uint8_t)(OLED_ADDR << 1));
    ok = sw_write_byte(control) && ok;
    for (i = 0; i < n; i++) {
        ok = sw_write_byte(buf[i]) && ok;
    }
    sw_stop();
    if (!ok) {
        oled_nacks++;
    }
}

/* Init: SSD1306 app note Fig. 2 (p.64), charge pump 8Dh 14h before AFh (p.62). */
static void oled_init(void)
{
    static const uint8_t cmds[] = {
        0xAE,                   /* display off */
        0xD5, 0x80,             /* oscillator frequency */
        0xA8, 0x3F,             /* MUX ratio 64 */
        0xD3, 0x00,             /* display offset 0 */
        0x40,                   /* display start line 0 */
        0x8D, 0x14,             /* charge pump on */
        0x20, 0x00,             /* horizontal addressing mode */
        OLED_SEG_REMAP,         /* segment re-map */
        OLED_COM_SCAN,          /* COM output scan direction */
        0xDA, OLED_COM_PINS,    /* COM pins hardware configuration */
        0x81, 0x7F,             /* contrast */
        0xA4,                   /* display follows RAM */
        0xA6,                   /* normal (not inverted) */
        0xAF,                   /* display on */
    };

    oled_write(0x00, cmds, sizeof(cmds));
}

/* Full-frame write: column 0-127, page 0-7, then 1024 data bytes in one transfer. */
static void oled_flush(void)
{
    static const uint8_t window[] = {0x21, 0x00, OLED_W - 1U, 0x22, 0x00, OLED_H / 8U - 1U};

    oled_write(0x00, window, sizeof(window));
    oled_write(0x40, fb, sizeof(fb));
}

static void px(uint32_t x, uint32_t y)
{
    if (x < OLED_W && y < OLED_H) {
        fb[(y / 8U) * OLED_W + x] |= (uint8_t)(1U << (y % 8U));
    }
}

static void fill(uint8_t v)
{
    uint32_t i;

    for (i = 0; i < sizeof(fb); i++) {
        fb[i] = v;
    }
}

static const char *draw_pattern(uint32_t n)
{
    uint32_t x, y, page;

    switch (n % 5U) {
    case 0:
        fill(0xFF);
        return "all on";
    case 1:
        fill(0x00);
        return "all off";
    case 2:
        for (page = 0; page < OLED_H / 8U; page++) {
            for (x = 0; x < OLED_W; x++) {
                fb[page * OLED_W + x] = (((x / 8U) + page) % 2U) ? 0xFF : 0x00;
            }
        }
        return "checkerboard 8x8";
    case 3:
        fill(0x00);
        for (x = 0; x < OLED_W; x++) {
            px(x, 0);
            px(x, OLED_H - 1U);
            px(x, (x * (OLED_H - 1U)) / (OLED_W - 1U));         /* diagonal */
        }
        for (y = 0; y < OLED_H; y++) {
            px(0, y);
            px(OLED_W - 1U, y);
        }
        return "border + diagonal";
    default:
        fill(0x00);
        for (x = 0; x < OLED_W; x++) {
            px(x, 15);
            px(x, 16);
        }
        return "lines at rows 15 and 16";
    }
}

int main(void)
{
    uint32_t t_scan, n_scan = 0;

    HAL_Init();
    SystemClock_Config();
    UART_Init();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    tx_str("# exp04 stage2 SSD1306 (Blue Pill), PCLK1=");
    tx_u32(HAL_RCC_GetPCLK1Freq());
    tx_str(" Hz");
    tx_line();
    report_lines("idle bus");
#if I2C_BITBANG
    tx_str("# mode: software I2C on PB10/PB11, half period ");
    tx_u32(SW_I2C_HALF_US);
    tx_str(" us");
    tx_line();
    sw_i2c_init();
#else
    I2C2_Init();
#endif

#if I2C_BITBANG
    /* Stage 2: one scan, then init and test patterns, PATTERN_MS each (non-blocking timing) */
    scan_sw(++n_scan);
    fill(0x00);
    oled_init();
    oled_flush();
    tx_str("# stage2 init done, nacks=");
    tx_u32(oled_nacks);
    tx_line();
    t_scan = HAL_GetTick() - PATTERN_MS;
    for (;;) {
        if (HAL_GetTick() - t_scan >= PATTERN_MS) {
            const char *name;

            t_scan = HAL_GetTick();
            name = draw_pattern(n_scan++);
            oled_flush();
            tx_str("pattern: ");
            tx_str(name);
            tx_str(" nacks=");
            tx_u32(oled_nacks);
            tx_line();
        }
    }
#else
    t_scan = HAL_GetTick() - SCAN_PERIOD_MS;    /* first scan at once */
    for (;;) {
        if (HAL_GetTick() - t_scan >= SCAN_PERIOD_MS) {
            t_scan = HAL_GetTick();
            scan(++n_scan);
        }
    }
#endif
}
