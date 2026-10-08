#ifndef EXP02_DHT11_H
#define EXP02_DHT11_H

#include <stdint.h>

typedef enum {
    DHT11_OK = 0,
    DHT11_CRC_ERR,
    DHT11_TIMEOUT           /* no valid 40-bit frame inside CAPTURE_WINDOW_MS */
} dht11_status_t;

typedef enum {
    DHT11_FAIL_NONE = 0,
    DHT11_FAIL_CAPTURE,     /* edge buffer overflow or TIM3 overcapture */
    DHT11_FAIL_NO_RESPONSE, /* no 80 us low + 80 us high response found */
    DHT11_FAIL_SHORT_FRAME, /* response found, fewer than 40 bits followed */
    DHT11_FAIL_LEVELS       /* bit cells did not alternate low/high */
} dht11_fail_t;

typedef struct {
    dht11_status_t status;
    dht11_fail_t fail;
    uint8_t  bits_valid;        /* 1: raw[], thr and w0/w1 are valid (OK or CRC error) */
    uint8_t  raw[5];            /* RH int, RH dec, T int, T dec, checksum (DHT p.5) */
    uint16_t thr_us;            /* derived high-width threshold: >= thr -> 1 */
    uint16_t w0_min, w0_max;    /* high widths classified as 0 */
    uint16_t w1_min, w1_max;    /* high widths classified as 1 */
    uint16_t lo_min, lo_max;    /* bit-start low widths */
    uint16_t start_low_us;      /* MCU start pulse, measured by TIM3 */
    uint16_t release_to_resp_us;/* MCU release -> sensor response falling edge */
    uint16_t resp_low_us, resp_high_us;
    uint16_t final_low_us;      /* low after the last bit; 0 if not captured */
    uint32_t edges;             /* edges captured in this read */
} dht11_result_t;

void dht11_init(void);

/* Non-blocking: call continuously. Returns 1 when a read has finished and *res is filled. */
int dht11_poll(dht11_result_t *res);

/* Edge widths of the last read (segment i = edge i -> edge i+1, in us). */
uint32_t dht11_segment_count(void);
uint16_t dht11_segment_width(uint32_t i);
uint8_t  dht11_segment_level(uint32_t i);

/* Called from TIM3_IRQHandler. */
void dht11_capture_isr(void);

#endif /* EXP02_DHT11_H */
