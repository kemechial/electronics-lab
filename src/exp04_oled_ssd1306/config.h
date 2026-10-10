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
#define I2C_SPEED_HZ        87800U          /* <= 88 kHz per ES096 Rev 15 2.8.5 p.19: CCR = 36 MHz / (2 x 87.8 kHz) = 205 -> 87.80 kHz (RM0008 p.781) */
#define I2C_SCAN_FIRST      0x08U
#define I2C_SCAN_LAST       0x77U
#define I2C_PROBE_TIMEOUT_MS 5U
#define SCAN_PERIOD_MS      2000U           /* rescan interval, non-blocking */

/* Software (bit-banged) I2C on the same pins. 1 = bit-bang, 0 = hardware I2C2.
 * Hardware I2C2 on this r2p0 clone puts a START instead of a STOP after a NACK (see README). */
#define I2C_BITBANG         1
#define SW_I2C_HALF_US      5U              /* half SCL period: 5 us -> ~100 kHz */
#define SW_I2C_STRETCH_US   1000U           /* max wait for SCL high (clock stretching) */


#endif /* EXP04_CONFIG_H */
