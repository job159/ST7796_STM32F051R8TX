/* Auto-generated anti-aliased 2bpp proportional fonts. See gen_fonts.ps1. */
#ifndef FONTS_H
#define FONTS_H
#include <stdint.h>

typedef struct {
  uint16_t off;    /* offset into bitmap data           */
  uint8_t  adv;    /* horizontal advance in pixels      */
  int8_t   xoff;   /* ink left offset from pen position */
  uint8_t  w, h;   /* ink bitmap size                   */
  int8_t   yoff;   /* ink top offset from line top      */
} GfxGlyph;

typedef struct {
  const uint8_t  *data;    /* 2bpp rows, ceil(w/4) bytes per row  */
  const GfxGlyph *glyphs;
  const char     *list;    /* NULL: range font [first..last]      */
  uint8_t first, last;
  uint8_t lineH;
} GfxFont;

extern const GfxFont FONT_SM;   /* 13 px labels / status      */
extern const GfxFont FONT_MD;   /* 17 px buttons / titles     */
extern const GfxFont FONT_BIG;  /* 34 px numeric readouts     */

#endif
