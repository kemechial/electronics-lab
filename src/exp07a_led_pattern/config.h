#ifndef EXP07A_CONFIG_H
#define EXP07A_CONFIG_H

#include <stdint.h>

/*
 * Experiment 7a: LED brightness pattern via hardware PWM (TIM3_CH3 on PB0).
 * Circuit unchanged from exp06: PB0 -> RB -> 2N2222A base, LED + R_LED from 3V3.
 */

#define PWM_HZ          1000U       /* PWM frequency */
#ifndef GAMMA
#define GAMMA           0           /* 0 = linear brightness, 1 = gamma 2.2 table */
#endif

/* One pattern segment: brightness ramps linearly (in percent, before gamma)
 * from start_percent to end_percent over duration_ms. */
typedef struct {
    uint8_t  start_percent;     /* 0..100 */
    uint8_t  end_percent;       /* 0..100 */
    uint32_t duration_ms;       /* 0 = segment skipped */
} segment_t;

/* Patterns: each is a table of segments, repeated forever.
 * To add a pattern: add a new table below and point ACTIVE_PATTERN at it. */
static const segment_t pattern_default[] = {
    {100,   0, 2000},   /* fade out over 2 s */
    {  0,   0, 1000},   /* off for 1 s */
    {  0, 100, 3000},   /* fade in over 3 s */
};

#define ACTIVE_PATTERN  pattern_default

#endif /* EXP07A_CONFIG_H */
