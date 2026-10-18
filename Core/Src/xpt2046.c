/* xpt2046.c - register-level SPI2 driver + filtering + calibration map */
#include "xpt2046.h"
#include "ili9488.h"

#define CMD_X  0xD0        /* 12-bit differential, PD=00 (PENIRQ on)     */
#define CMD_Y  0x90

static TouchCal s_cal;

static void spi2_init(void)
{
  GPIO_InitTypeDef io = {0};

  __HAL_RCC_SPI2_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  io.Mode      = GPIO_MODE_AF_PP;
  io.Pull      = GPIO_NOPULL;
  io.Speed     = GPIO_SPEED_FREQ_HIGH;
  io.Alternate = GPIO_AF0_SPI2;
  io.Pin       = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOB, &io);

  /* master, fPCLK/32 = 1.5 MHz, 8-bit, soft NSS, full duplex */
  SPI2->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | (4u << SPI_CR1_BR_Pos);
  SPI2->CR2 = (7u << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH;
  SPI2->CR1 |= SPI_CR1_SPE;
}

static uint8_t spi2_xfer(uint8_t b)
{
  while (!(SPI2->SR & SPI_SR_TXE)) { }
  *(volatile uint8_t *)&SPI2->DR = b;
  while (!(SPI2->SR & SPI_SR_RXNE)) { }
  return *(volatile uint8_t *)&SPI2->DR;
}

static uint16_t read_ch(uint8_t cmdb)
{
  spi2_xfer(cmdb);
  uint16_t hi = spi2_xfer(0x00);
  uint16_t lo = spi2_xfer(0x00);
  return (uint16_t)(((hi << 8) | lo) >> 3) & 0x0FFF;
}

/* median-of-5 on one channel, discarding first (settling) conversion */
static uint16_t sample_ch(uint8_t cmdb)
{
  uint16_t v[5];
  (void)read_ch(cmdb);
  for (int i = 0; i < 5; i++) v[i] = read_ch(cmdb);
  for (int i = 1; i < 5; i++) {             /* insertion sort           */
    uint16_t k = v[i]; int j = i - 1;
    while (j >= 0 && v[j] > k) { v[j + 1] = v[j]; j--; }
    v[j + 1] = k;
  }
  return v[2];
}

void touch_init(void)
{
  spi2_init();
  if (!board_cal_load(&s_cal)) {
    /* typical values for these 3.5" SPI modules in landscape (0x28)    */
    s_cal.swap = 1;
    s_cal.rx1 = 550;  s_cal.rx2 = 3650;     /* raw driving screen X     */
    s_cal.ry1 = 3500; s_cal.ry2 = 600;      /* raw driving screen Y     */
  }
}

bool touch_pressed(void)
{
  return (TP_IRQ_PORT->IDR & TP_IRQ_PIN) == 0;
}

bool touch_raw(uint16_t *rx, uint16_t *ry)
{
  if (!touch_pressed()) return false;
  PIN_LO(TP_CS_PORT, TP_CS_PIN);
  uint16_t x = sample_ch(CMD_X);
  uint16_t y = sample_ch(CMD_Y);
  PIN_HI(TP_CS_PORT, TP_CS_PIN);
  if (!touch_pressed()) return false;       /* released mid-read        */
  *rx = x; *ry = y;
  return true;
}

static int16_t map_axis(int32_t raw, int32_t r1, int32_t r2,
                        int32_t s1, int32_t s2, int32_t lim)
{
  if (r2 == r1) return (int16_t)s1;
  int32_t v = s1 + (raw - r1) * (s2 - s1) / (r2 - r1);
  if (v < 0) v = 0;
  if (v > lim) v = lim;
  return (int16_t)v;
}

bool touch_get(int16_t *x, int16_t *y)
{
  uint16_t rx, ry;
  if (!touch_raw(&rx, &ry)) return false;
  uint16_t sx = s_cal.swap ? ry : rx;       /* raw feeding screen X     */
  uint16_t sy = s_cal.swap ? rx : ry;
  *x = map_axis(sx, s_cal.rx1, s_cal.rx2, TP_REF_X1, TP_REF_X2, LCD_W - 1);
  *y = map_axis(sy, s_cal.ry1, s_cal.ry2, TP_REF_Y1, TP_REF_Y2, LCD_H - 1);
  return true;
}

void touch_setCal(const TouchCal *c) { s_cal = *c; }
TouchCal touch_getCal(void)          { return s_cal; }
