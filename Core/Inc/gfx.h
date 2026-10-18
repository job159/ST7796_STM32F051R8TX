/**
 * gfx.h - anti-aliased drawing engine for ILI9488 (no framebuffer).
 *
 * All shapes blend against a caller-supplied background color, so AA works
 * without reading GRAM back. Colors are 0xRRGGBB.
 */
#ifndef GFX_H
#define GFX_H

#include "ili9488.h"
#include "fonts.h"

uint32_t gfx_blend(uint32_t bg, uint32_t fg, uint8_t a);          /* a 0..255 */
uint32_t gfx_lerp(uint32_t c1, uint32_t c2, uint32_t num, uint32_t den);

int32_t  gfx_sin(int32_t deg);     /* q10 (-1024..1024), any angle          */
int32_t  gfx_cos(int32_t deg);
uint32_t gfx_isqrt(uint32_t v);

void gfx_hline(int x, int y, int w, uint32_t c);
void gfx_vline(int x, int y, int h, uint32_t c);
void gfx_fillGradV(int x, int y, int w, int h, uint32_t top, uint32_t bot);

/* AA rounded rectangle, solid or vertical-gradient fill, over solid bg    */
void gfx_rrect(int x, int y, int w, int h, int r, uint32_t fill, uint32_t bg);
void gfx_rrectGrad(int x, int y, int w, int h, int r,
                   uint32_t top, uint32_t bot, uint32_t bg);
/* bordered panel: border ring + inner fill (draw content afterwards)      */
void gfx_panel(int x, int y, int w, int h, int r,
               uint32_t border, uint32_t fill, uint32_t bg);

void gfx_circle(int cx, int cy, int r, uint32_t fill, uint32_t bg);
void gfx_ring(int cx, int cy, int r, int thick,
              uint32_t col, uint32_t inner, uint32_t bg);

/* text: y is the top of the line box; returns advance width in px         */
int  gfx_text(const GfxFont *f, int x, int y, const char *s,
              uint32_t fg, uint32_t bg);
int  gfx_textT(const GfxFont *f, int x, int y, const char *s,
               uint32_t fg, uint32_t bg, int tracking);
int  gfx_textW(const GfxFont *f, const char *s, int tracking);
int  gfx_textCenter(const GfxFont *f, int cx, int y, const char *s,
                    uint32_t fg, uint32_t bg);
int  gfx_glyphTop(const GfxFont *f, char ch);   /* ink yoff, for cap align */

#endif
