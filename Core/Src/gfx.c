/* gfx.c - AA drawing engine (blends against known background, no FB) */
#include "gfx.h"
#include <string.h>

static uint8_t lb[LCD_W * 3];          /* one scanline buffer             */

/* ------------------------------------------------------------ helpers */
uint32_t gfx_blend(uint32_t bg, uint32_t fg, uint8_t a)
{
  /* alpha 0..255 -> 0..256 so channels stay separated under >>8 */
  uint32_t aa = (uint32_t)a + (a >> 7);
  uint32_t rb = ((bg & 0xFF00FF) * (256 - aa) + (fg & 0xFF00FF) * aa) >> 8;
  uint32_t g  = ((bg & 0x00FF00) * (256 - aa) + (fg & 0x00FF00) * aa) >> 8;
  return (rb & 0xFF00FF) | (g & 0x00FF00);
}

uint32_t gfx_lerp(uint32_t c1, uint32_t c2, uint32_t num, uint32_t den)
{
  if (den == 0) return c1;
  if (num > den) num = den;
  return gfx_blend(c1, c2, (uint8_t)((num * 255u) / den));
}

uint32_t gfx_isqrt(uint32_t v)
{
  uint32_t r = 0, b = 1u << 30;
  while (b > v) b >>= 2;
  while (b) {
    if (v >= r + b) { v -= r + b; r = (r >> 1) + b; }
    else            { r >>= 1; }
    b >>= 2;
  }
  return r;
}

int32_t gfx_sin(int32_t deg)               /* Bhaskara I approximation */
{
  deg %= 360; if (deg < 0) deg += 360;
  int32_t s = 1;
  if (deg >= 180) { deg -= 180; s = -1; }
  int32_t t = deg * (180 - deg);           /* 0..8100                  */
  return s * ((t << 12) / (40500 - t));
}

int32_t gfx_cos(int32_t deg) { return gfx_sin(deg + 90); }

static inline void put3(uint8_t *p, uint32_t c)
{
  p[0] = (uint8_t)(c >> 16); p[1] = (uint8_t)(c >> 8); p[2] = (uint8_t)c;
}

/* ------------------------------------------------------- basic fills */
void gfx_hline(int x, int y, int w, uint32_t c) { lcd_fillRect(x, y, w, 1, c); }
void gfx_vline(int x, int y, int h, uint32_t c) { lcd_fillRect(x, y, 1, h, c); }

void gfx_fillGradV(int x, int y, int w, int h, uint32_t top, uint32_t bot)
{
  if (w <= 0 || h <= 0) return;
  lcd_begin();
  lcd_setWindow((uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h);
  for (int i = 0; i < h; i++)
    lcd_fillN(gfx_lerp(top, bot, (uint32_t)i, (uint32_t)(h - 1)), (uint32_t)w);
  lcd_end();
}

/* --------------------------------------- AA rounded rect / panel core
 * bw = border width (0 for plain fill). Vertical gradient supported.   */
static void rrect_core(int x, int y, int w, int h, int r, int bw,
                       uint32_t border, uint32_t ftop, uint32_t fbot,
                       uint32_t bg)
{
  if (w <= 0 || h <= 0) return;
  if (r > w / 2) r = w / 2;
  if (r > h / 2) r = h / 2;
  int32_t r8  = r << 8;
  int32_t rI  = r - bw; if (rI < 0) rI = 0;
  int32_t rI8 = rI << 8;

  lcd_begin();
  lcd_setWindow((uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h);

  for (int yy = 0; yy < h; yy++) {
    uint32_t fill = (ftop == fbot) ? ftop
                  : gfx_lerp(ftop, fbot, (uint32_t)yy, (uint32_t)(h - 1));
    int32_t yc8 = (yy << 8) + 128;
    int32_t dy8 = 0;
    if      (yc8 < r8)              dy8 = r8 - yc8;
    else if (yc8 > (h << 8) - r8)   dy8 = yc8 - ((h << 8) - r8);

    /* outer left boundary in q8 */
    int32_t bxO8 = 0;
    if (dy8 > 0) {
      if (dy8 >= r8) bxO8 = r8;
      else bxO8 = r8 - (int32_t)gfx_isqrt((uint32_t)(r8 * r8 - dy8 * dy8));
    }
    /* inner (fill) left boundary */
    int32_t bxI8;
    if (bw == 0)            bxI8 = bxO8;
    else if (dy8 >= rI8)    bxI8 = (w << 8) / 2;     /* border only row  */
    else {
      int32_t hwI = (int32_t)gfx_isqrt((uint32_t)(rI8 * rI8 - dy8 * dy8));
      bxI8 = r8 - hwI;
      if (bxI8 < bxO8 + (bw << 8)) bxI8 = bxO8 + (bw << 8);
    }
    if (bxI8 > (w << 8) / 2) bxI8 = (w << 8) / 2;

    int ixO = bxO8 >> 8;  uint8_t covO = (uint8_t)(255 - (bxO8 & 0xFF));
    int ixI = bxI8 >> 8;  uint8_t covI = (uint8_t)(255 - (bxI8 & 0xFF));
    uint32_t edgeCol = bw ? border : fill;

    uint8_t *p = lb;
    for (int xx = 0; xx < w; xx++) {
      int mx = (xx < w - 1 - xx) ? xx : (w - 1 - xx);   /* mirrored x   */
      uint32_t c;
      if      (mx <  ixO) c = bg;
      else if (mx == ixO) c = gfx_blend(bg, edgeCol, covO);
      else if (mx <  ixI) c = edgeCol;
      else if (mx == ixI && bw) c = gfx_blend(border, fill, covI);
      else                c = fill;
      put3(p, c); p += 3;
    }
    lcd_pushBuf(lb, (uint32_t)w * 3u);
  }
  lcd_end();
}

void gfx_rrect(int x, int y, int w, int h, int r, uint32_t fill, uint32_t bg)
{ rrect_core(x, y, w, h, r, 0, 0, fill, fill, bg); }

void gfx_rrectGrad(int x, int y, int w, int h, int r,
                   uint32_t top, uint32_t bot, uint32_t bg)
{ rrect_core(x, y, w, h, r, 0, 0, top, bot, bg); }

void gfx_panel(int x, int y, int w, int h, int r,
               uint32_t border, uint32_t fill, uint32_t bg)
{ rrect_core(x, y, w, h, r, 1, border, fill, fill, bg); }

/* ------------------------------------------------------- AA circles  */
void gfx_ring(int cx, int cy, int r, int thick,
              uint32_t col, uint32_t inner, uint32_t bg)
{
  int d = 2 * r;
  int32_t r8 = r << 8, rI = r - thick;
  if (rI < 0) rI = 0;
  int32_t rI8 = rI << 8;

  lcd_begin();
  lcd_setWindow((uint16_t)(cx - r), (uint16_t)(cy - r), (uint16_t)d, (uint16_t)d);
  for (int yy = 0; yy < d; yy++) {
    int32_t dy8 = (yy << 8) + 128 - r8;
    if (dy8 < 0) dy8 = -dy8;
    int32_t hwO = (dy8 >= r8) ? 0
                : (int32_t)gfx_isqrt((uint32_t)(r8 * r8 - dy8 * dy8));
    int32_t hwI = (dy8 >= rI8 || thick >= r) ? -1
                : (int32_t)gfx_isqrt((uint32_t)(rI8 * rI8 - dy8 * dy8));
    int32_t bxO8 = r8 - hwO;
    int32_t bxI8 = (hwI < 0) ? (d << 8) / 2 : r8 - hwI;
    int ixO = bxO8 >> 8;  uint8_t covO = (uint8_t)(255 - (bxO8 & 0xFF));
    int ixI = bxI8 >> 8;  uint8_t covI = (uint8_t)(255 - (bxI8 & 0xFF));

    uint8_t *p = lb;
    for (int xx = 0; xx < d; xx++) {
      int mx = (xx < d - 1 - xx) ? xx : (d - 1 - xx);
      uint32_t c;
      if      (mx <  ixO) c = bg;
      else if (mx == ixO) c = gfx_blend(bg, col, covO);
      else if (mx <  ixI) c = col;
      else if (mx == ixI) c = gfx_blend(col, inner, covI);
      else                c = inner;
      put3(p, c); p += 3;
    }
    lcd_pushBuf(lb, (uint32_t)d * 3u);
  }
  lcd_end();
}

void gfx_circle(int cx, int cy, int r, uint32_t fill, uint32_t bg)
{ gfx_ring(cx, cy, r, r, fill, fill, bg); }

/* ------------------------------------------------------------- text  */
static const GfxGlyph *glyph_of(const GfxFont *f, char ch)
{
  if (f->list) {
    for (int i = 0; f->list[i]; i++)
      if (f->list[i] == ch) return &f->glyphs[i];
    return 0;
  }
  uint8_t c = (uint8_t)ch;
  if (c < f->first || c > f->last) return 0;
  return &f->glyphs[c - f->first];
}

int gfx_glyphTop(const GfxFont *f, char ch)
{
  const GfxGlyph *g = glyph_of(f, ch);
  return g ? g->yoff : 0;
}

int gfx_textT(const GfxFont *f, int x, int y, const char *s,
              uint32_t fg, uint32_t bg, int tracking)
{
  /* 2bpp shade LUT for this fg/bg pair */
  uint32_t lut[4] = { bg, gfx_blend(bg, fg, 85), gfx_blend(bg, fg, 170), fg };
  int pen = x;

  for (; *s; s++) {
    const GfxGlyph *g = glyph_of(f, *s);
    if (!g) { pen += f->lineH / 3 + tracking; continue; }
    if (g->w && g->h) {
      int gx = pen + g->xoff, gy = y + g->yoff;
      int rowB = (g->w + 3) / 4;
      const uint8_t *d = f->data + g->off;
      lcd_begin();
      lcd_setWindow((uint16_t)gx, (uint16_t)gy, g->w, g->h);
      for (int row = 0; row < g->h; row++) {
        uint8_t *p = lb;
        for (int xx = 0; xx < g->w; xx++) {
          uint8_t q = (d[row * rowB + (xx >> 2)] >> (6 - 2 * (xx & 3))) & 3u;
          put3(p, lut[q]); p += 3;
        }
        lcd_pushBuf(lb, (uint32_t)g->w * 3u);
      }
      lcd_end();
    }
    pen += g->adv + tracking;
  }
  return pen - x;
}

int gfx_text(const GfxFont *f, int x, int y, const char *s,
             uint32_t fg, uint32_t bg)
{ return gfx_textT(f, x, y, s, fg, bg, 0); }

int gfx_textW(const GfxFont *f, const char *s, int tracking)
{
  int w = 0;
  for (; *s; s++) {
    const GfxGlyph *g = glyph_of(f, *s);
    w += (g ? g->adv : f->lineH / 3) + tracking;
  }
  return w;
}

int gfx_textCenter(const GfxFont *f, int cx, int y, const char *s,
                   uint32_t fg, uint32_t bg)
{
  int w = gfx_textW(f, s, 0);
  return gfx_text(f, cx - w / 2, y, s, fg, bg);
}
