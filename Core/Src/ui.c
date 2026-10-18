/* ui.c - NEXUS sensor console: animations, dashboard, LED/motor logic */
#include "ui.h"
#include "sensors.h"

/* ------------------------------------------------------------- theme */
#define C_BG     0x0A101Du
#define C_BAR    0x0D1626u
#define C_CARD   0x141E30u
#define C_EDGE   0x243350u
#define C_INSET  0x1D2740u
#define C_TEXT   0xE8EEF8u
#define C_MUT    0x8291ACu
#define C_ACC    0x2AD4FFu
#define C_ACC2   0x3D7BFFu
#define C_OK     0x35D07Au
#define C_WARN   0xFFB020u
#define C_BAD    0xFF4D5Eu
#define C_KNOB   0xF2F6FCu

#define FW_STR   "FW 2.0.0"

#define S_OHM    "\xEA"            /* ohm glyph in the generated fonts  */
#define S_DEG    "\xB0"            /* degree glyph                      */

/* ------------------------------------------------------------ layout */
#define BAR_H     30
#define STRIP_Y   296
#define CARD_R    10

#define TC_Y      38               /* top metric cards                  */
#define TC_H      88
#define TC_W      148
#define TC_X(i)   (10 + (i) * 156)

#define MC_Y      134              /* middle row                        */
#define MC_H      76
#define MOTC_X    10
#define MOTC_W    228
#define RNGC_X    246
#define RNGC_W    224

#define BC_Y      216              /* bottom row                        */
#define BC_H      72
#define TRC_X     10
#define TRC_W     228
#define SYSC_X    246
#define SYSC_W    224

#define SEG_N     18               /* motor bar: 10 deg per segment     */
#define SPARK_N   102
#define SPARK_X   22
#define SPARK_Y   242
#define SPARK_H   38

/* ------------------------------------------------------------- state */
static struct {
  LightVal li[2];
  DistVal  dv;
  uint16_t angle;                  /* motor angle / duty                */
  int8_t   dir;                    /* +1 stepping up, -1 stepping down  */
  bool     ledG, ledR;             /* physical status LED states        */
  bool     blinkOn;                /* blink phase in NEAR zone          */
  uint32_t upSec;
  int      bigTop;
  uint8_t  segLit;
  uint8_t  stripId;                /* last drawn strip variant          */
  uint8_t  spark[SPARK_N];
  char     rStr[2][8], vStr[2][8], dStr[8];
} S;

/* --------------------------------------------------------- utilities */
static void fmt_u(char *b, uint32_t v)
{
  char t[10]; int n = 0;
  do { t[n++] = (char)('0' + v % 10u); v /= 10u; } while (v);
  while (n) *b++ = t[--n];
  *b = 0;
}

static void fmt_1dp(char *b, int v10)      /* 523 -> "52.3"             */
{
  if (v10 < 0) v10 = 0;
  fmt_u(b, (uint32_t)(v10 / 10));
  while (*b) b++;
  b[0] = '.'; b[1] = (char)('0' + v10 % 10); b[2] = 0;
}

static void fmt_2dp(char *b, int v100)     /* 543 -> "5.43"             */
{
  if (v100 < 0) v100 = 0;
  fmt_u(b, (uint32_t)(v100 / 100));
  while (*b) b++;
  b[0] = '.'; b[1] = (char)('0' + (v100 / 10) % 10);
  b[2] = (char)('0' + v100 % 10); b[3] = 0;
}

/* resistance in kilo-ohms with adaptive precision */
static void fmt_kohm(char *b, uint32_t ohm, bool open)
{
  if (open)                { b[0]=b[1]=b[2]='-'; b[3]=0; }
  else if (ohm >= 100000u) fmt_u(b, ohm / 1000u);          /* "132"     */
  else if (ohm >= 10000u)  fmt_1dp(b, (int)(ohm / 100u));  /* "43.1"    */
  else                     fmt_2dp(b, (int)(ohm / 10u));   /* "5.43"    */
}

static void fmt_mv(char *b, uint16_t mv)   /* 1234 -> "1.234"           */
{
  b[0] = (char)('0' + (mv / 1000u) % 10u); b[1] = '.';
  b[2] = (char)('0' + (mv / 100u) % 10u);
  b[3] = (char)('0' + (mv / 10u) % 10u);
  b[4] = (char)('0' + mv % 10u); b[5] = 0;
}

static void fmt_clock(char *b, uint32_t sec)
{
  uint32_t h = (sec / 3600u) % 100u, m = (sec / 60u) % 60u, s = sec % 60u;
  b[0]=(char)('0'+h/10); b[1]=(char)('0'+h%10); b[2]=':';
  b[3]=(char)('0'+m/10); b[4]=(char)('0'+m%10); b[5]=':';
  b[6]=(char)('0'+s/10); b[7]=(char)('0'+s%10); b[8]=0;
}

static bool str_same(const char *a, const char *b)
{
  while (*a || *b) { if (*a != *b) return false; a++; b++; }
  return true;
}

static void str_cpy(char *d, const char *s)
{
  while ((*d++ = *s++)) { }
}

/* ================================================== BOOT ANIMATION == */
static void boot_screen(void)
{
  lcd_fillRect(0, 0, LCD_W, LCD_H, C_BG);

  for (int i = 0; i <= 20; i++) { board_backlight((uint16_t)(i * 5)); HAL_Delay(12); }

  int cx = LCD_W / 2, cy = 128;

  for (int r = 16; r <= 42; r += 6) {
    gfx_ring(cx, cy, r, 4, C_ACC, C_BG, C_BG);
    HAL_Delay(26);
  }
  gfx_ring(cx, cy, 42, 4, C_ACC, C_BG, C_BG);
  gfx_circle(cx, cy, 8, C_ACC, C_BG);

  int px = -100, py = -100;
  for (int a = -90; a <= 450; a += 18) {
    int dx = cx + (int)((54 * gfx_cos(a)) >> 10);
    int dy = cy + (int)((54 * gfx_sin(a)) >> 10);
    if (px > -100) gfx_circle(px, py, 4, C_BG, C_BG);
    gfx_circle(dx, dy, 3, C_KNOB, C_BG);
    px = dx; py = dy;
    HAL_Delay(16);
  }
  gfx_circle(px, py, 4, C_BG, C_BG);

  for (int st = 1; st <= 5; st++) {
    uint32_t a = (uint32_t)(st * 51u);
    uint32_t c1 = gfx_blend(C_BG, C_TEXT, (uint8_t)a);
    uint32_t c2 = gfx_blend(C_BG, C_MUT,  (uint8_t)a);
    int w = gfx_textW(&FONT_MD, "N E X U S", 2);
    gfx_textT(&FONT_MD, cx - w / 2, 196, "N E X U S", c1, C_BG, 2);
    gfx_textCenter(&FONT_SM, cx, 226, "SENSOR CONSOLE", c2, C_BG);
    HAL_Delay(40);
  }
  gfx_textCenter(&FONT_SM, cx, 300, FW_STR, gfx_blend(C_BG, C_MUT, 140), C_BG);
  HAL_Delay(350);
}

/* ============================================== LOADING ANIMATION == */
static const char *load_msgs[] = {
  "Initializing SPI bus",
  "ADC calibration done",
  "Ultrasonic ranger armed",
  "Loading interface assets",
  "Starting services",
};

static void spinner(int cx, int cy, int phase)
{
  for (int i = 0; i < 8; i++) {
    int age = (phase - i) & 7;
    uint8_t a = (uint8_t)(255 - age * 30);
    int dx = cx + (int)((22 * gfx_cos(i * 45)) >> 10);
    int dy = cy + (int)((22 * gfx_sin(i * 45)) >> 10);
    gfx_circle(dx, dy, 3, gfx_blend(C_BG, C_ACC, a), C_BG);
  }
}

static void loading_screen(void)
{
  lcd_fillRect(0, 0, LCD_W, LCD_H, C_BG);

  int bx = 120, by = 210, bw = 240, bh = 10;
  gfx_rrect(bx, by, bw, bh, 5, C_INSET, C_BG);
  gfx_textCenter(&FONT_SM, LCD_W / 2, 300, FW_STR,
                 gfx_blend(C_BG, C_MUT, 140), C_BG);

  int shown = -1, phase = 0;
  for (int pct = 0; pct <= 100; pct++) {
    uint32_t d = (pct % 25 == 0) ? 120 : (pct % 7 == 0) ? 44 : 14;

    int msg = pct / 21; if (msg > 4) msg = 4;
    if (msg != shown) {
      shown = msg;
      lcd_fillRect(100, 248, 280, 20, C_BG);
      gfx_textCenter(&FONT_SM, LCD_W / 2, 248, load_msgs[msg], C_MUT, C_BG);
    }

    int fw = 4 + (bw - 4) * pct / 100;
    gfx_rrectGrad(bx + 2, by + 2, fw - 4 < 6 ? 6 : fw - 4, bh - 4, 3,
                  C_ACC, C_ACC2, C_INSET);

    char t[8]; fmt_u(t, (uint32_t)pct);
    char s[8] = ""; int k = 0;
    while (t[k]) { s[k] = t[k]; k++; }
    s[k] = '%'; s[k + 1] = 0;
    lcd_fillRect(216, 156, 60, 26, C_BG);
    gfx_textCenter(&FONT_MD, LCD_W / 2, 156, s, C_TEXT, C_BG);

    if (pct % 4 == 0) { spinner(LCD_W / 2, 120, phase); phase = (phase + 1) & 7; }
    HAL_Delay(d);
  }
  HAL_Delay(150);
}

/* ================================================== CALIBRATION UI == */
static void cal_cross(int x, int y, uint32_t c)
{
  gfx_hline(x - 10, y, 21, c);
  gfx_vline(x, y - 10, 21, c);
  gfx_ring(x, y, 8, 2, c, C_BG, C_BG);
}

static bool cal_capture(int sx, int sy, uint16_t *rx, uint16_t *ry)
{
  cal_cross(sx, sy, C_ACC);
  uint32_t rxs = 0, rys = 0; int n = 0;
  uint32_t t0 = HAL_GetTick();
  while (HAL_GetTick() - t0 < 15000u) {
    uint16_t a, b;
    if (touch_raw(&a, &b)) {
      rxs += a; rys += b; n++;
      if (n >= 16) {
        while (touch_pressed()) { }
        *rx = (uint16_t)(rxs / (uint32_t)n);
        *ry = (uint16_t)(rys / (uint32_t)n);
        cal_cross(sx, sy, C_OK);
        HAL_Delay(180);
        cal_cross(sx, sy, C_BG);
        return true;
      }
    } else if (n) { n = 0; rxs = rys = 0; }
    HAL_Delay(8);
  }
  return false;
}

static void calibration_screen(void)
{
  lcd_fillRect(0, 0, LCD_W, LCD_H, C_BG);
  gfx_textCenter(&FONT_MD, LCD_W / 2, 130, "TOUCH CALIBRATION", C_TEXT, C_BG);
  gfx_textCenter(&FONT_SM, LCD_W / 2, 162, "Tap the center of each target",
                 C_MUT, C_BG);

  uint16_t x1r, y1r, x2r, y2r, x3r, y3r;
  if (!cal_capture(TP_REF_X1, TP_REF_Y1, &x1r, &y1r)) return;
  if (!cal_capture(TP_REF_X2, TP_REF_Y1, &x2r, &y2r)) return;
  if (!cal_capture(LCD_W / 2, TP_REF_Y2, &x3r, &y3r)) return;

  int32_t dx = (int32_t)x2r - (int32_t)x1r; if (dx < 0) dx = -dx;
  int32_t dy = (int32_t)y2r - (int32_t)y1r; if (dy < 0) dy = -dy;

  TouchCal c;
  c.swap = (dy > dx) ? 1 : 0;
  if (c.swap) {
    c.rx1 = (int16_t)y1r; c.rx2 = (int16_t)y2r;
    c.ry1 = (int16_t)((x1r + x2r) / 2); c.ry2 = (int16_t)x3r;
  } else {
    c.rx1 = (int16_t)x1r; c.rx2 = (int16_t)x2r;
    c.ry1 = (int16_t)((y1r + y2r) / 2); c.ry2 = (int16_t)y3r;
  }
  touch_setCal(&c);
  bool saved = board_cal_save(&c);

  lcd_fillRect(0, 120, LCD_W, 80, C_BG);
  gfx_textCenter(&FONT_MD, LCD_W / 2, 140,
                 saved ? "CALIBRATION SAVED" : "CALIBRATION APPLIED",
                 C_OK, C_BG);
  HAL_Delay(700);
}

/* ==================================================== DASHBOARD ==== */
static void draw_topbar(void)
{
  lcd_fillRect(0, 0, LCD_W, BAR_H, C_BAR);
  gfx_hline(0, BAR_H, LCD_W, C_EDGE);
  gfx_circle(18, 15, 4, C_ACC, C_BAR);
  int w = gfx_textT(&FONT_MD, 30, 3, "NEXUS", C_TEXT, C_BAR, 1);
  gfx_text(&FONT_SM, 30 + w + 10, 7, "SENSOR CONSOLE", C_MUT, C_BAR);
  gfx_text(&FONT_SM, 336, 7, "48 MHz", C_MUT, C_BAR);
  gfx_circle(402, 15, 4, C_OK, C_BAR);
}

static void draw_clock(void)
{
  char b[10];
  fmt_clock(b, S.upSec);
  lcd_fillRect(414, 7, 60, 16, C_BAR);
  gfx_text(&FONT_SM, 414, 7, b, C_TEXT, C_BAR);
}

/* strip variants: 0 valid, 1 near, 2 no target, 3 too close */
static void draw_strip(uint8_t id)
{
  static const char *txt[] = { "RANGE VALID", "TARGET NEAR",
                               "NO TARGET IN RANGE", "TARGET TOO CLOSE" };
  static const uint32_t col[] = { C_OK, C_WARN, C_BAD, C_BAD };
  S.stripId = id;
  lcd_fillRect(0, STRIP_Y + 1, LCD_W, LCD_H - STRIP_Y - 1, C_BAR);
  gfx_hline(0, STRIP_Y, LCD_W, C_EDGE);
  gfx_circle(18, STRIP_Y + 12, 4, col[id], C_BAR);
  gfx_text(&FONT_SM, 30, STRIP_Y + 4, txt[id],
           id == 0 ? C_MUT : col[id], C_BAR);
  int w = gfx_textW(&FONT_SM, FW_STR "  48 MHz", 0);
  gfx_text(&FONT_SM, LCD_W - 14 - w, STRIP_Y + 4, FW_STR "  48 MHz",
           C_MUT, C_BAR);
}

static void card_frame(int x, int y, int w, int h, const char *title)
{
  gfx_panel(x, y, w, h, CARD_R, C_EDGE, C_CARD, C_BG);
  if (title) gfx_textT(&FONT_SM, x + 12, y + 7, title, C_MUT, C_CARD, 1);
}

/* ------------------------------------------------ light cards (0,1) */
static void draw_light_card(int i, bool force)
{
  int x = TC_X(i), y = TC_Y;
  uint32_t ac = i ? C_ACC2 : C_ACC;
  LightVal v = S.li[i];

  char r[8], u[8];
  fmt_kohm(r, v.ohm, v.open);
  fmt_mv(u, v.mv);

  if (force || !str_same(r, S.rStr[i])) {
    str_cpy(S.rStr[i], r);
    lcd_fillRect(x + 10, y + 28, TC_W - 20, 30, C_CARD);
    int vw = gfx_text(&FONT_BIG, x + 12, y + 30 - S.bigTop, r,
                      v.open ? C_MUT : C_TEXT, C_CARD);
    if (!v.open)
      gfx_text(&FONT_MD, x + 12 + vw + 5, y + 40, "k" S_OHM, C_MUT, C_CARD);
  }
  if (force || !str_same(u, S.vStr[i])) {
    str_cpy(S.vStr[i], u);
    char line[12];
    str_cpy(line, u);
    int k = 0; while (line[k]) k++;
    line[k] = ' '; line[k + 1] = 'V'; line[k + 2] = 0;
    lcd_fillRect(x + 12, y + 56, 80, 16, C_CARD);
    gfx_text(&FONT_SM, x + 12, y + 55, line, C_TEXT, C_CARD);

    int frac = (int)v.pct * 124 / 100;
    gfx_rrect(x + 12, y + 74, 124, 6, 3, C_INSET, C_CARD);
    if (frac > 6)
      gfx_rrect(x + 12, y + 74, frac, 6, 3, ac, C_CARD);
  }
}

/* ------------------------------------------------------ distance card */
static uint32_t dist_color(void)
{
  return S.dv.state == DIST_VALID ? C_OK
       : S.dv.state == DIST_NEAR  ? C_WARN : C_BAD;
}

static const char *dist_word(void)
{
  if (S.dv.state == DIST_NEAR)  return "NEAR";
  if (S.dv.state == DIST_VALID) return "VALID";
  if (S.dv.mm != 0xFFFF && S.dv.mm < DIST_MIN_MM) return "TOO CLOSE";
  return "NO TARGET";
}

static void draw_dist_card(bool force)
{
  int x = TC_X(2), y = TC_Y;
  char d[8];
  if (S.dv.mm == 0xFFFF) { d[0]=d[1]=d[2]='-'; d[3]=0; }
  else fmt_1dp(d, S.dv.mm);                  /* mm -> cm with 1 dp      */

  if (!force && str_same(d, S.dStr)) return;
  str_cpy(S.dStr, d);

  uint32_t sc = dist_color();
  lcd_fillRect(x + 10, y + 28, TC_W - 20, 30, C_CARD);
  int vw = gfx_text(&FONT_BIG, x + 12, y + 30 - S.bigTop, d,
                    S.dv.mm == 0xFFFF ? C_MUT : C_TEXT, C_CARD);
  if (S.dv.mm != 0xFFFF)
    gfx_text(&FONT_MD, x + 12 + vw + 5, y + 40, "cm", C_MUT, C_CARD);

  lcd_fillRect(x + 12, y + 56, TC_W - 24, 16, C_CARD);
  gfx_text(&FONT_SM, x + 12, y + 55, dist_word(), sc, C_CARD);

  int frac = 0;
  if (S.dv.mm != 0xFFFF && S.dv.mm <= DIST_MAX_MM)
    frac = (int)S.dv.mm * 124 / DIST_MAX_MM;
  gfx_rrect(x + 12, y + 74, 124, 6, 3, C_INSET, C_CARD);
  if (frac > 6)
    gfx_rrect(x + 12, y + 74, frac, 6, 3, sc, C_CARD);
}

/* -------------------------------------------------------- motor card */
static void draw_seg(int idx, bool lit)
{
  int x = 22 + idx * 10, y = MC_Y + 44;
  uint32_t c = lit ? gfx_lerp(C_ACC, C_ACC2, (uint32_t)idx, SEG_N - 1)
                   : C_INSET;
  gfx_rrect(x, y, 8, 16, 2, c, C_CARD);
}

static void draw_dir_tri(int x, int y, bool up, uint32_t c)
{
  for (int i = 0; i < 6; i++) {
    int w2 = up ? i : 5 - i;
    gfx_hline(x - w2, y + i, 2 * w2 + 1, c);
  }
}

static void draw_motor_card(bool force)
{
#if MOTOR_SERVO
  uint8_t lit = (uint8_t)((S.angle + 5) / 10);
#else
  uint8_t lit = (uint8_t)(((uint32_t)S.angle * SEG_N + 50) / 100);
#endif
  if (lit > SEG_N) lit = SEG_N;

  if (force) {
    for (int i = 0; i < SEG_N; i++) draw_seg(i, i < lit);
  } else if (lit != S.segLit) {
    int lo = lit < S.segLit ? lit : S.segLit;
    int hi = lit > S.segLit ? lit : S.segLit;
    for (int i = lo; i < hi; i++) draw_seg(i, i < lit);
  }
  S.segLit = lit;

  /* value, right-aligned */
  char b[8];
  fmt_u(b, S.angle);
  int k = 0; while (b[k]) k++;
#if MOTOR_SERVO
  b[k] = '\xB0'; b[k + 1] = 0;
#else
  b[k] = '%'; b[k + 1] = 0;
#endif
  lcd_fillRect(MOTC_X + 120, MC_Y + 6, 96, 32, C_CARD);
  int w = gfx_textW(&FONT_BIG, b, 0);
  int vx = MOTC_X + MOTC_W - 14 - w;
  gfx_text(&FONT_BIG, vx, MC_Y + 10 - S.bigTop, b, C_TEXT, C_CARD);
  draw_dir_tri(vx - 14, MC_Y + 18, S.dir > 0,
               S.dir > 0 ? C_ACC : C_WARN);
}

/* -------------------------------------------------- range status card */
static void draw_range_dot(int which)      /* 0 green, 1 red            */
{
  int cy = MC_Y + (which ? 58 : 34);
  bool on = which ? S.ledR : S.ledG;
  uint32_t c = which ? C_BAD : C_OK;
  gfx_ring(RNGC_X + 24, cy, 7, 2,
           on ? c : C_INSET,
           on ? gfx_blend(C_CARD, c, 210) : C_CARD, C_CARD);
}

static void draw_range_word(void)
{
  const char *wd = S.dv.state == DIST_VALID ? "OK"
                 : S.dv.state == DIST_NEAR  ? "NEAR" : "----";
  lcd_fillRect(RNGC_X + 130, MC_Y + 32, 82, 24, C_CARD);
  int w = gfx_textW(&FONT_MD, wd, 1);
  gfx_textT(&FONT_MD, RNGC_X + RNGC_W - 16 - w, MC_Y + 32, wd,
            dist_color(), C_CARD, 1);
}

static void draw_range_card(void)
{
  card_frame(RNGC_X, MC_Y, RNGC_W, MC_H, "RANGE STATUS");
  gfx_text(&FONT_SM, RNGC_X + 40, MC_Y + 26, "VALID 2-60 cm", C_MUT, C_CARD);
  gfx_text(&FONT_SM, RNGC_X + 40, MC_Y + 50, "FAULT / OUT", C_MUT, C_CARD);
  draw_range_dot(0);
  draw_range_dot(1);
  draw_range_word();
}

/* --------------------------------------------------------- sparkline */
static void draw_spark(void)
{
  uint32_t dim = gfx_blend(C_CARD, C_ACC, 55);
  for (int i = 0; i < SPARK_N; i++) {
    int top = SPARK_H - 1 - S.spark[i];
    lcd_begin();
    lcd_setWindow((uint16_t)(SPARK_X + i * 2), SPARK_Y, 2, SPARK_H);
    for (int yy = 0; yy < SPARK_H; yy++) {
      uint32_t c = (yy < top) ? C_CARD : (yy == top) ? C_ACC : dim;
      lcd_px(c); lcd_px(c);
    }
    lcd_end();
  }
}

static void spark_push(uint8_t v)
{
  for (int i = 0; i < SPARK_N - 1; i++) S.spark[i] = S.spark[i + 1];
  S.spark[SPARK_N - 1] = v >= SPARK_H ? SPARK_H - 1 : v;
}

/* -------------------------------------------------------- system card */
static void draw_aux_dot(void)
{
  bool on = board_led_get(LED_AUX1);
  gfx_ring(SYSC_X + 24, BC_Y + 36, 7, 2,
           on ? C_ACC : C_INSET,
           on ? gfx_blend(C_CARD, C_ACC, 210) : C_CARD, C_CARD);
}

static void draw_sys_card(void)
{
  card_frame(SYSC_X, BC_Y, SYSC_W, BC_H, "SYSTEM");
  draw_aux_dot();
  gfx_text(&FONT_SM, SYSC_X + 40, BC_Y + 28, "AUX1  HOLD BTN", C_MUT, C_CARD);
  gfx_text(&FONT_SM, SYSC_X + 12, BC_Y + 50, FW_STR "  SPI 24 MHz",
           C_MUT, C_CARD);
}

static void build_dashboard(void)
{
  lcd_fillRect(0, 0, LCD_W, LCD_H, C_BG);
  draw_topbar();
  draw_clock();

  card_frame(TC_X(0), TC_Y, TC_W, TC_H, "LIGHT 1");
  card_frame(TC_X(1), TC_Y, TC_W, TC_H, "LIGHT 2");
  card_frame(TC_X(2), TC_Y, TC_W, TC_H, "DISTANCE");
  draw_light_card(0, true);
  draw_light_card(1, true);
  draw_dist_card(true);

#if MOTOR_SERVO
  card_frame(MOTC_X, MC_Y, MOTC_W, MC_H, "SERVO OUTPUT");
  gfx_text(&FONT_SM, MOTC_X + 12, MC_Y + 24, "BTN +10" S_DEG " STEP",
           gfx_blend(C_CARD, C_MUT, 160), C_CARD);
#else
  card_frame(MOTC_X, MC_Y, MOTC_W, MC_H, "MOTOR PWM");
  gfx_text(&FONT_SM, MOTC_X + 12, MC_Y + 24, "BTN +10% STEP",
           gfx_blend(C_CARD, C_MUT, 160), C_CARD);
#endif
  S.segLit = 0;
  draw_motor_card(true);

  draw_range_card();

  card_frame(TRC_X, BC_Y, TRC_W, BC_H, "DISTANCE TREND");
  draw_spark();

  draw_sys_card();
  draw_strip(S.stripId);
}

/* ============================================ SENSOR / LED LOGIC ==== */
static void apply_leds(void)
{
  bool g, r;
  switch (S.dv.state) {
    case DIST_VALID: g = true;      r = false; break;
    case DIST_NEAR:  g = S.blinkOn; r = false; break;
    default:         g = false;     r = true;  break;
  }
  if (g != S.ledG) { S.ledG = g; board_led(LED_GREEN, g); draw_range_dot(0); }
  if (r != S.ledR) { S.ledR = r; board_led(LED_RED, r);   draw_range_dot(1); }
}

static uint8_t strip_id_now(void)
{
  if (S.dv.state == DIST_VALID) return 0;
  if (S.dv.state == DIST_NEAR)  return 1;
  if (S.dv.mm != 0xFFFF && S.dv.mm < DIST_MIN_MM) return 3;
  return 2;
}

static void sensors_refresh(void)
{
  S.li[0] = light_get(0);
  S.li[1] = light_get(1);
  DistVal nd = dist_get();
  bool stateChanged = nd.state != S.dv.state;
  S.dv = nd;

  draw_light_card(0, false);
  draw_light_card(1, false);
  draw_dist_card(false);

  if (stateChanged) {
    draw_range_word();
    apply_leds();
  }
  uint8_t sid = strip_id_now();
  if (sid != S.stripId) draw_strip(sid);
}

/* ==================================================== BUTTON ======= */
static void motor_step(void)
{
#if MOTOR_SERVO
  const int16_t maxv = 180;
#else
  const int16_t maxv = 100;
#endif
  int16_t a = (int16_t)S.angle + S.dir * 10;
  if (a >= maxv) { a = maxv; S.dir = -1; }
  else if (a <= 0) { a = 0; S.dir = 1; }
  S.angle = (uint16_t)a;
  motor_set(S.angle);
  draw_motor_card(false);
}

/* short press = motor step, long press (>=600 ms) = toggle AUX1 LED   */
static void handle_button(void)
{
  static uint8_t hist = 0;
  static bool held = false;
  static uint32_t t0 = 0;
  hist = (uint8_t)((hist << 1) | (board_btn() ? 1u : 0u));
  if ((hist & 0x07u) == 0x07u && !held) {
    held = true;
    t0 = HAL_GetTick();
  } else if ((hist & 0x07u) == 0u && held) {
    held = false;
    if (HAL_GetTick() - t0 >= 600u) {
      board_led(LED_AUX1, !board_led_get(LED_AUX1));
      draw_aux_dot();
    } else {
      motor_step();
    }
  }
}

/* ========================================================== PUBLIC == */
void ui_init(void)
{
  S.angle = 90;
  S.dir   = 1;
  S.dv.mm = 0xFFFF;
  S.dv.state = DIST_INVALID;
  S.stripId = 2;
  S.bigTop = gfx_glyphTop(&FONT_BIG, '0');
  for (int i = 0; i < SPARK_N; i++) S.spark[i] = 0;
  S.rStr[0][0] = S.rStr[1][0] = S.vStr[0][0] = S.vStr[1][0] = 0;
  S.dStr[0] = 0;

  boot_screen();

  if (board_btn()) calibration_screen();

  loading_screen();
  board_backlight(100);

  S.li[0] = light_get(0);
  S.li[1] = light_get(1);
  S.dv    = dist_get();
  S.stripId = strip_id_now();
  S.ledG = S.ledR = false;
  build_dashboard();
  apply_leds();
  draw_range_word();
}

void ui_task(void)
{
  static uint32_t t10, t100, t250, t400, t1000;
  uint32_t now = HAL_GetTick();

  if (now - t10 >= 10u) {
    t10 = now;
    sensors_task();
    handle_button();
  }
  if (now - t250 >= 250u) {                  /* NEAR-zone green blink   */
    t250 = now;
    if (S.dv.state == DIST_NEAR) {
      S.blinkOn = !S.blinkOn;
      apply_leds();
    } else if (!S.blinkOn) {
      S.blinkOn = true;
    }
  }
  if (now - t100 >= 100u) {
    t100 = now;
    sensors_refresh();
  }
  if (now - t400 >= 400u) {
    t400 = now;
    uint16_t mm = (S.dv.mm == 0xFFFF) ? 0 : S.dv.mm;
    spark_push((uint8_t)(((uint32_t)mm * (SPARK_H - 2)) / (DIST_MAX_MM + 20)));
    draw_spark();
  }
  if (now - t1000 >= 1000u) {
    t1000 = now;
    S.upSec++;
    draw_clock();
  }
}
