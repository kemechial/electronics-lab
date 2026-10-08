#ifndef EXP02_CONFIG_H
#define EXP02_CONFIG_H

/*
 * Experiment 2: DHT11 single-wire read with TIM3 input capture.
 * Sources: DHT = docs/ref/DHT11.PDF, DS = DS9716 Rev 11, RM = RM0368 Rev 6.
 */

#ifndef DEBUG_ENABLED
#define DEBUG_ENABLED       0           /* 1 = USART1 output */
#endif

/* --- Hardware --- */
#define PULLUP_OHM          4700UL      /* external DATA pull-up to 3V3 (DHT p.5: ~5 kOhm below 20 m); not measured */
#define SUPPLY_MV           3300UL      /* nominal; DHT p.5: 3-5.5 V, DHT p.8 table specified at 5 V */

#define DHT_PORT            GPIOA
#define DHT_PIN             GPIO_PIN_6      /* TIM3_CH1 = AF2 (DS p.44, Table 9) */
#define DHT_PIN_POS         6U
#define DHT_PIN_AF          GPIO_AF2_TIM3

/* --- Protocol timing (DHT p.5-8) --- */
#define POWERUP_WAIT_MS     1000UL      /* DHT p.5: no start signal within 1 s of power-up */
#define READ_PERIOD_MS      2000UL      /* DHT p.8: sampling period >= 1 s */
#define START_LOW_MS        20UL        /* DHT p.6: >= 18 ms; 1 ms tick -> actual >= 19 ms (measured by TIM3) */
#define CAPTURE_WINDOW_MS   10UL        /* frame ~4 ms (DHT p.5); worst case from Fig. 3-5 ~5.1 ms */

/* --- Capture / decoder --- */
#define TIM_TICK_HZ         1000000UL   /* TIM3 at 1 us per tick */
#define TIM_IC_FILTER       3U          /* IC1F = 0011: fCK_INT, N = 8 (~95 ns at 84 MHz, RM p.364) */
#define EDGE_BUF_LEN        100U        /* a full frame is 84 or 85 edges */
#define RESP_MIN_US         40U         /* response low/high window; nominal 80 us (DHT p.7) */
#define RESP_MAX_US         120U
#define MIN_CLUSTER_GAP_US  15U         /* high-width spread below this = one cluster: thr = mean bit-start low */

#if READ_PERIOD_MS < 1000UL
#error "DHT p.8: sampling period must be >= 1 s"
#endif
#if START_LOW_MS < 19UL
#error "DHT p.6: start low >= 18 ms; with a 1 ms tick START_LOW_MS must be >= 19"
#endif

/* --- Board --- */
#define LED_PORT            GPIOC
#define LED_PIN             GPIO_PIN_13     /* active-low */
#define UART_TX_PORT        GPIOA
#define UART_TX_PIN         GPIO_PIN_9      /* USART1_TX, AF7 */

#endif /* EXP02_CONFIG_H */
