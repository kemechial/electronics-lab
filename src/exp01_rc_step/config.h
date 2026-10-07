#ifndef EXP01_CONFIG_H
#define EXP01_CONFIG_H

/*
 * Experiment 1: RC step response.
 * PB0 -> R -> node -> C -> GND, node -> PA1 (ADC1_IN1). Debug UART: USART1 TX PA9.
 * All values are integers: no floating point anywhere in this experiment.
 */

#ifndef DEBUG_ENABLED
#define DEBUG_ENABLED       0           /* 1 = USART1 CSV + summary output */
#endif

/* --- Component values (calculated vs measured: docs/exp01-rc-step/README.md) --- */
#define R_NOMINAL_OHM       9830UL      /* measured with DMM */
#define C_NOMINAL_NF        10460UL     /* measured: 10.46 uF */
#define VDDA_MV             3266UL      /* measured at the 3V3 pin; fallback if VREFINT measurement is invalid */

/* Expected tau = R * C. Ohm * nF = ns; /1000 (rounded) = us.
 * 9830 * 10460 = 102,821,800 ns -> 102,822 us (fits in uint32). */
#define TAU_EXPECTED_US     ((R_NOMINAL_OHM * C_NOMINAL_NF + 500UL) / 1000UL)

/* --- Sampling --- */
#define SAMPLE_PERIOD_US    1000UL      /* TIM2 TRGO period = ADC sample spacing */
#define N_SAMPLES           800U        /* 800 ms = 7.8 tau -> 99.96 % settled */
#define SETTLE_AVG_N        16U         /* last N samples averaged as final value */
#define TAU_STEP_PERMILLE   632         /* 63.2 % of the step (charge: 63.2 %, discharge: 36.8 %) */

/* --- Sequence timing --- */
#define DISCHARGE_MS        3000UL      /* PB0 LOW before charge: ~29 tau */
#define HOLD_HIGH_MS        500UL       /* PB0 held HIGH between charge and discharge capture */
#define CYCLE_PERIOD_MS     10000UL     /* one full sequence every 10 s */
#define BLINK_EVERY_N       50U         /* toggle PC13 every 50 samples during capture */

/* --- ADC --- */
#define ADC_FULL_SCALE      4095UL
#define VREFINT_AVG_N       16U         /* VREFINT conversions averaged per cycle for VDDA */
#define VDDA_VALID_MIN_MV   1700UL      /* F401 VDDA operating range; outside -> use VDDA_MV */
#define VDDA_VALID_MAX_MV   3600UL
#define PRE_EDGE_AVG_N      4U          /* node conversions averaged right before each PB0 edge */

/* --- Pins --- */
#define STEP_PORT           GPIOB
#define STEP_PIN            GPIO_PIN_0
#define LED_PORT            GPIOC
#define LED_PIN             GPIO_PIN_13     /* active-low */
#define SENSE_PORT          GPIOA
#define SENSE_PIN           GPIO_PIN_1      /* ADC1_IN1 */
#define SENSE_CHANNEL       ADC_CHANNEL_1
#define UART_TX_PORT        GPIOA
#define UART_TX_PIN         GPIO_PIN_9      /* USART1_TX, AF7 */

#endif /* EXP01_CONFIG_H */
