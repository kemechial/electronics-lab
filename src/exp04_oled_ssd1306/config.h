#ifndef EXP04_CONFIG_H
#define EXP04_CONFIG_H

/*
 * Experiment 4: SSD1306 128x64 I2C OLED (two-colour Y-B) on the Blue Pill
 * (STM32F103C8 clone, CPUID r2p0, DEV_ID 0x410).
 * Sources: DS5319 Rev 20, RM0008 Rev 21, SSD1306 Rev 1.1 (docs/ref/INDEX.md).
 * PB6 stays reserved for the exp07a PWM: not used here.
 */

/* --- Wiring (user) --- */
#define I2C_PORT            GPIOB
#define I2C_SCL_PIN         GPIO_PIN_10     /* I2C2_SCL, FT, LQFP48 pin 21 (DS5319 Table 5 p.30) */
#define I2C_SDA_PIN         GPIO_PIN_11     /* I2C2_SDA, FT, LQFP48 pin 22 (DS5319 Table 5 p.30) */
#define UART_TX_PORT        GPIOA
#define UART_TX_PIN         GPIO_PIN_9      /* USART1_TX -> FTDI RX (COM5), 115200 8N1 */

/* --- I2C --- */
#define I2C_SPEED_HZ        100000U         /* Stage 1: 100 kHz Sm. CCR 180, TRISE 37 at PCLK1 36 MHz (RM0008 p.781-782) */
#define I2C_SCAN_FIRST      0x08U
#define I2C_SCAN_LAST       0x77U
#define I2C_PROBE_TIMEOUT_MS 5U
#define SCAN_PERIOD_MS      2000U           /* rescan interval, non-blocking */


#endif /* EXP04_CONFIG_H */
