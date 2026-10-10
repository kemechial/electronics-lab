/*
 * Experiment 4, Stage 1: I2C2 bus check and address scan (0x08-0x77), Blue Pill (STM32F103C8 clone).
 * Result lines on USART1 (PA9, 115200 8N1). Expected SSD1306 address 0x3C or 0x3D
 * (0111 10 SA0, SSD1306 p.19).
 * Clock: HSE 8 MHz x 9 = 72 MHz SYSCLK, APB1 /2 = 36 MHz = PCLK1 for I2C2.
 * I2C 100 kHz Sm: CCR = 36 MHz / (2 x 100 kHz) = 180 (RM0008 p.781-782),
 *                 TRISE = 1000 ns / 27.8 ns + 1 = 37 (RM0008 p.782); HAL computes both.
 * Bus recovery (BUSY/TIMEOUT or a line stuck low): 9 SCL pulses as GPIO, STOP, I2C re-init.
 * Every occurrence is logged. The sequence is not yet cited: ES096 (F103 errata) is not in docs/ref.
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

static void I2C2_Init(void)
{
    GPIO_InitTypeDef g = {0};

    g.Pin = I2C_SCL_PIN | I2C_SDA_PIN;
    g.Mode = GPIO_MODE_AF_OD;                   /* pull-ups on the OLED module (assumption) */
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(I2C_PORT, &g);

    __HAL_RCC_I2C2_CLK_ENABLE();
    __HAL_RCC_I2C2_FORCE_RESET();               /* clear a stuck BUSY flag in the peripheral */
    __HAL_RCC_I2C2_RELEASE_RESET();
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

/* Bus recovery: release a slave holding SDA low by clocking SCL 9 times as
 * open-drain GPIO, then generate a STOP (SDA low -> high while SCL high), then
 * reset and re-init I2C2. Logged with a running count. */
static void bus_recover(const char *reason)
{
    GPIO_InitTypeDef g = {0};
    uint32_t i;

    recoveries++;
    tx_str("# RECOVERY ");
    tx_u32(recoveries);
    tx_str(": ");
    tx_str(reason);
    tx_line();

    HAL_I2C_DeInit(&hi2c2);
    HAL_GPIO_WritePin(I2C_PORT, I2C_SCL_PIN | I2C_SDA_PIN, GPIO_PIN_SET);
    g.Pin = I2C_SCL_PIN | I2C_SDA_PIN;
    g.Mode = GPIO_MODE_OUTPUT_OD;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(I2C_PORT, &g);

    for (i = 0; i < RECOVERY_CLOCKS; i++) {
        HAL_GPIO_WritePin(I2C_PORT, I2C_SCL_PIN, GPIO_PIN_RESET);
        half_bit_wait();
        HAL_GPIO_WritePin(I2C_PORT, I2C_SCL_PIN, GPIO_PIN_SET);
        half_bit_wait();
    }
    /* STOP: SDA low while SCL high, then SDA high */
    HAL_GPIO_WritePin(I2C_PORT, I2C_SDA_PIN, GPIO_PIN_RESET);
    half_bit_wait();
    HAL_GPIO_WritePin(I2C_PORT, I2C_SDA_PIN, GPIO_PIN_SET);
    half_bit_wait();

    report_lines("after recovery");
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

int main(void)
{
    uint32_t t_scan, n_scan = 0;
    uint8_t scl, sda;

    HAL_Init();
    SystemClock_Config();
    UART_Init();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    tx_str("# exp04 stage1 I2C2 scan (Blue Pill), PCLK1=");
    tx_u32(HAL_RCC_GetPCLK1Freq());
    tx_str(" Hz");
    tx_line();
    report_lines("idle bus");
    read_lines(&scl, &sda);
    if (!scl || !sda) {
        bus_recover("line low before init");
    } else {
        I2C2_Init();
    }

    t_scan = HAL_GetTick() - SCAN_PERIOD_MS;    /* first scan at once */
    for (;;) {
        if (HAL_GetTick() - t_scan >= SCAN_PERIOD_MS) {
            t_scan = HAL_GetTick();
            scan(++n_scan);
        }
    }
}
