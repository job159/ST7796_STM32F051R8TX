/**
 * ui.h - NEXUS industrial HMI: boot / loading animations, dashboard,
 *        touch calibration, LED controls.
 */
#ifndef UI_H
#define UI_H

#include "gfx.h"
#include "xpt2046.h"

void ui_init(void);      /* boot + loading animation, builds dashboard   */
void ui_task(void);      /* call continuously from the main loop         */

#endif
