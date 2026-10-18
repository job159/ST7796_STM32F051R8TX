/**
 * sensors.h - dual LDR light channels (ADC1) + HC-SR04 ultrasonic ranger.
 *
 * Light channels (voltage divider, fixed resistor LDR_RFIX = 1073 ohm):
 *     3V3 --- LDR --- [PA1 / PA2] --- 1073R --- GND
 *   ADC reads the divider node:  R_ldr = RFIX * (4095 - adc) / adc.
 *   (If your divider is flipped - LDR on the GND side - set LDR_ON_TOP 0.)
 *
 * Ultrasonic (HC-SR04):
 *   TRIG -> PA9 (3.3 V pulse is fine)     ECHO -> PA8 (5 V tolerant pin)
 *   VCC  -> 5V                            GND  -> common GND
 *   Echo width is captured with EXTI8 interrupts + TIM16 (1 MHz), so
 *   ranging never blocks the UI. Median-of-5 + EMA filtering.
 *
 * Valid range: DIST_MIN_MM .. DIST_MAX_MM (out of it -> invalid);
 * below DIST_NEAR_MM (but valid) -> "near" zone.
 */
#ifndef SENSORS_H
#define SENSORS_H

#include "board.h"

#define LDR_RFIX      1073u        /* fixed divider resistor, ohms       */
#define LDR_ON_TOP    1            /* 1: LDR between 3V3 and node        */

#define DIST_MIN_MM   20
#define DIST_NEAR_MM  100
#define DIST_MAX_MM   600

typedef struct {
  uint16_t mv;                     /* node voltage in millivolts         */
  uint32_t ohm;                    /* computed LDR resistance            */
  uint8_t  pct;                    /* mv / 3300, for bars (0..100)       */
  bool     open;                   /* reading too extreme to trust       */
} LightVal;

typedef enum { DIST_INVALID = 0, DIST_VALID, DIST_NEAR } DistState;

typedef struct {
  uint16_t  mm;                    /* filtered distance (0xFFFF = none)  */
  DistState state;
} DistVal;

void     sensors_init(void);
void     sensors_task(void);       /* call every ~10 ms                  */
LightVal light_get(uint8_t idx);   /* idx 0 = PA1, 1 = PA2               */
DistVal  dist_get(void);
void     hcsr04_echo_isr(void);    /* from EXTI4_15 handler              */

#endif
