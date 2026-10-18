/* ili9488.c - register-level SPI1 driver for ILI9488, TX-only @ 24 MHz */
#include "ili9488.h"

static void spi1_init(void)
{
  GPIO_InitTypeDef io = {0};

  __HAL_RCC_SPI1_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  io.Mode      = GPIO_MODE_AF_PP;
  io.Pull      = GPIO_NOPULL;
  io.Speed     = GPIO_SPEED_FREQ_HIGH;
  io.Alternate = GPIO_AF0_SPI1;
  io.Pin       = GPIO_PIN_5 | GPIO_PIN_7;          /* SCK, MOSI           */
  HAL_GPIO_Init(GPIOA, &io);

  /* LCD_SPI_BR: 0 -> 24 MHz, 1 -> 12 MHz.
   * If the panel shows noise/garbage on long jumper wires, set back to 1. */
#ifndef LCD_SPI_BR
#define LCD_SPI_BR 0
#endif
  SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI     /* soft NSS    */
            | SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE            /* TX only     */
            | (LCD_SPI_BR << SPI_CR1_BR_Pos);
  SPI1->CR2 = (7u << SPI_CR2_DS_Pos);                      /* 8-bit       */
  SPI1->CR1 |= SPI_CR1_SPE;
}

static inline void spi1_flush(void)
{
  while (SPI1->SR & SPI_SR_FTLVL) { }
  while (SPI1->SR & SPI_SR_BSY) { }
}

static void wr_cmd(uint8_t c)
{
  spi1_flush();
  PIN_LO(LCD_DC_PORT, LCD_DC_PIN);
  lcd_byte(c);
  spi1_flush();
  PIN_HI(LCD_DC_PORT, LCD_DC_PIN);
}

static void wr_data(const uint8_t *d, uint32_t n)
{
  while (n--) lcd_byte(*d++);
}

static void cmd(uint8_t c, const uint8_t *d, uint32_t n)
{
  wr_cmd(c);
  wr_data(d, n);
}

void lcd_begin(void) { PIN_LO(LCD_CS_PORT, LCD_CS_PIN); }
void lcd_end(void)   { spi1_flush(); PIN_HI(LCD_CS_PORT, LCD_CS_PIN); }

void lcd_init(void)
{
  spi1_init();

  /* hardware reset */
  PIN_HI(LCD_RST_PORT, LCD_RST_PIN); HAL_Delay(5);
  PIN_LO(LCD_RST_PORT, LCD_RST_PIN); HAL_Delay(15);
  PIN_HI(LCD_RST_PORT, LCD_RST_PIN); HAL_Delay(120);

  static const uint8_t mad[] = {LCD_MADCTL};
  static const uint8_t pix[] = {0x66};             /* 18-bit RGB666       */

#if LCD_ST7796
  static const uint8_t csc1[] = {0xC3};            /* unlock command set  */
  static const uint8_t csc2[] = {0x96};
  static const uint8_t invc[] = {0x01};            /* 1-dot inversion     */
  static const uint8_t dfc[]  = {0x80,0x02,0x3B};
  static const uint8_t doca[] = {0x40,0x8A,0x00,0x00,0x29,0x19,0xA5,0x33};
  static const uint8_t pwr2[] = {0x06};
  static const uint8_t pwr3[] = {0xA7};
  static const uint8_t vcom[] = {0x18};
  static const uint8_t pgam[] = {0xF0,0x09,0x0B,0x06,0x04,0x15,0x2F,
                                 0x54,0x42,0x3C,0x17,0x14,0x18,0x1B};
  static const uint8_t ngam[] = {0xE0,0x09,0x0B,0x06,0x04,0x03,0x2B,
                                 0x43,0x42,0x3B,0x16,0x14,0x17,0x1B};
  static const uint8_t lck1[] = {0x3C};            /* lock command set    */
  static const uint8_t lck2[] = {0x69};

  lcd_begin();
  wr_cmd(0x11);                                    /* sleep out           */
  lcd_end();
  HAL_Delay(120);
  lcd_begin();
  cmd(0xF0, csc1, sizeof csc1);
  cmd(0xF0, csc2, sizeof csc2);
  cmd(0x36, mad,  sizeof mad);
  cmd(0x3A, pix,  sizeof pix);
  cmd(0xB4, invc, sizeof invc);
  cmd(0xB6, dfc,  sizeof dfc);
  cmd(0xE8, doca, sizeof doca);
  cmd(0xC1, pwr2, sizeof pwr2);
  cmd(0xC2, pwr3, sizeof pwr3);
  cmd(0xC5, vcom, sizeof vcom);
  lcd_end();
  HAL_Delay(120);
  lcd_begin();
  cmd(0xE0, pgam, sizeof pgam);
  cmd(0xE1, ngam, sizeof ngam);
  cmd(0xF0, lck1, sizeof lck1);
  cmd(0xF0, lck2, sizeof lck2);
#if LCD_INVERT
  wr_cmd(0x21);
#endif
  lcd_end();
  HAL_Delay(120);
  lcd_begin();
  wr_cmd(0x29);                                    /* display on          */
  lcd_end();
  HAL_Delay(20);

#else /* ILI9488 */
  static const uint8_t pgam[] = {0x00,0x03,0x09,0x08,0x16,0x0A,0x3F,0x78,
                                 0x4C,0x09,0x0A,0x08,0x16,0x1A,0x0F};
  static const uint8_t ngam[] = {0x00,0x16,0x19,0x03,0x0F,0x05,0x32,0x45,
                                 0x46,0x04,0x0E,0x0D,0x35,0x37,0x0F};
  static const uint8_t pwr1[] = {0x17,0x15};
  static const uint8_t pwr2[] = {0x41};
  static const uint8_t vcom[] = {0x00,0x12,0x80};
  static const uint8_t ifmd[] = {0x80};            /* SDO not used        */
  static const uint8_t frat[] = {0xA0};            /* ~60 Hz              */
  static const uint8_t inv[]  = {0x02};            /* 2-dot inversion     */
  static const uint8_t dfc[]  = {0x02,0x02};
  static const uint8_t sime[] = {0x00};
  static const uint8_t adj[]  = {0xA9,0x51,0x2C,0x82};

  lcd_begin();
  cmd(0xE0, pgam, sizeof pgam);
  cmd(0xE1, ngam, sizeof ngam);
  cmd(0xC0, pwr1, sizeof pwr1);
  cmd(0xC1, pwr2, sizeof pwr2);
  cmd(0xC5, vcom, sizeof vcom);
  cmd(0x36, mad,  sizeof mad);
  cmd(0x3A, pix,  sizeof pix);
  cmd(0xB0, ifmd, sizeof ifmd);
  cmd(0xB1, frat, sizeof frat);
  cmd(0xB4, inv,  sizeof inv);
  cmd(0xB6, dfc,  sizeof dfc);
  cmd(0xE9, sime, sizeof sime);
  cmd(0xF7, adj,  sizeof adj);
#if LCD_INVERT
  wr_cmd(0x21);
#endif
  wr_cmd(0x11);                                    /* sleep out           */
  lcd_end();
  HAL_Delay(120);
  lcd_begin();
  wr_cmd(0x29);                                    /* display on          */
  lcd_end();
  HAL_Delay(20);
#endif
}

void lcd_setWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  uint16_t x1 = x + w - 1, y1 = y + h - 1;
  uint8_t ca[4] = { (uint8_t)(x >> 8), (uint8_t)x, (uint8_t)(x1 >> 8), (uint8_t)x1 };
  uint8_t pa[4] = { (uint8_t)(y >> 8), (uint8_t)y, (uint8_t)(y1 >> 8), (uint8_t)y1 };
  cmd(0x2A, ca, 4);
  cmd(0x2B, pa, 4);
  wr_cmd(0x2C);                                    /* RAMWR               */
}

void lcd_pushBuf(const uint8_t *buf, uint32_t len)
{
  while (len--) {
    while (!(SPI1->SR & SPI_SR_TXE)) { }
    *(volatile uint8_t *)&SPI1->DR = *buf++;
  }
}

void lcd_fillN(uint32_t rgb, uint32_t count)
{
  uint8_t r = (uint8_t)(rgb >> 16), g = (uint8_t)(rgb >> 8), b = (uint8_t)rgb;
  while (count--) {
    lcd_byte(r);
    lcd_byte(g);
    lcd_byte(b);
  }
}

void lcd_fillRect(int x, int y, int w, int h, uint32_t rgb)
{
  if (w <= 0 || h <= 0) return;
  lcd_begin();
  lcd_setWindow((uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h);
  lcd_fillN(rgb, (uint32_t)w * (uint32_t)h);
  lcd_end();
}
