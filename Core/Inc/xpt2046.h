/**
 * xpt2046.h - resistive touch controller on SPI2 (1.5 MHz), PB11 = PENIRQ.
 *
 * Calibration: 3-point (auto axis-swap detection), persisted to flash.
 * Reference points used by the calibration UI and the mapping math:
 *   X refs: screen x = 30 and 450     Y refs: screen y = 30 and 290
 */
#ifndef XPT2046_H
#define XPT2046_H

#include "board.h"

#define TP_REF_X1   30
#define TP_REF_X2   450
#define TP_REF_Y1   30
#define TP_REF_Y2   290

void touch_init(void);
bool touch_pressed(void);
bool touch_get(int16_t *x, int16_t *y);            /* filtered + mapped   */
bool touch_raw(uint16_t *rx, uint16_t *ry);        /* for calibration     */

void     touch_setCal(const TouchCal *c);
TouchCal touch_getCal(void);

#endif
