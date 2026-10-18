/**
 * ili9488.h - 320x480 TFT on 4-wire SPI (DBI type C), 18-bit color.
 * Supports ST7796S and ILI9488 controllers (select below).
 *
 * SPI1 is driven at register level in TX-only half-duplex mode, 24 MHz.
 * Colors are plain 0xRRGGBB; the panel latches the top 6 bits per channel.
 */
#ifndef ILI9488_H
#define ILI9488_H

#include "board.h"

/* 1 = ST7796S (blue 9-pin SPI module, "ST7796" silkscreen)
 * 0 = ILI9488  (modules with XPT2046 touch, 14-pin header)          */
#ifndef LCD_ST7796
#define LCD_ST7796  1
#endif

/* set to 1 if colors show inverted (some IPS ST7796 panels)         */
#ifndef LCD_INVERT
#define LCD_INVERT  0
#endif

#define LCD_W  480
#define LCD_H  320

/* MADCTL: 0x28 = landscape, 0xE8 = landscape flipped 180 deg;
 * clear bit 0x08 (BGR) if red and blue appear swapped               */
#ifndef LCD_MADCTL
#define LCD_MADCTL  0x28
#endif

void lcd_init(void);
void lcd_setWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

/* stream interface: setWindow -> begin -> bytes/fill -> end */
void lcd_begin(void);
void lcd_end(void);
void lcd_pushBuf(const uint8_t *buf, uint32_t len);
void lcd_fillN(uint32_t rgb, uint32_t count);      /* count = pixel count */

/* single-shot solid rectangle (window + fill + end) */
void lcd_fillRect(int x, int y, int w, int h, uint32_t rgb);

/* raw SPI byte push, valid between lcd_begin/lcd_end */
static inline void lcd_byte(uint8_t b)
{
  while (!(SPI1->SR & SPI_SR_TXE)) { }
  *(volatile uint8_t *)&SPI1->DR = b;
}

static inline void lcd_px(uint32_t rgb)
{
  lcd_byte((uint8_t)(rgb >> 16));
  lcd_byte((uint8_t)(rgb >> 8));
  lcd_byte((uint8_t)rgb);
}

#endif
