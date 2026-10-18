/**
 * board.h - Board support for TFT_ILI9488_STM32F051 HMI
 *
 * Target : STM32F051R8T6 @ 48 MHz (HSI/2 x PLL12), e.g. STM32F0DISCOVERY
 *
 * Wiring A - ST7796 9-pin SPI module (no touch), LCD_ST7796 = 1:
 *   GND -> GND      RST   -> PB8
 *   VCC -> 3.3V     DC    -> PB7
 *   SCL -> PA5      CS    -> PB6
 *   SDA -> PA7      BL    -> PB9 (PWM) or 3.3V
 *   SDA-O -> leave open
 *   IM jumpers must select 4-line SPI: IM0=1 IM1=1 IM2=1
 *
 * Wiring B - ILI9488 module with XPT2046 touch, LCD_ST7796 = 0:
 *   LCD  CS    -> PB6          T_CLK -> PB13 (SPI2 SCK)
 *   LCD  DC/RS -> PB7          T_CS  -> PB12
 *   LCD  RESET -> PB8          T_DIN -> PB15 (SPI2 MOSI)
 *   LCD  LED   -> PB9 (PWM)    T_DO  -> PB14 (SPI2 MISO)
 *   LCD  SDI   -> PA7 (MOSI)   T_IRQ -> PB11
 *   LCD  SCK   -> PA5          LCD SDO -> leave open
 *
 * Common:
 *   LED1 -> PC8 (Discovery LD4)   LED2  -> PC9 (Discovery LD3)
 *   GREEN LED -> PC6 (+ ~330R)    RED LED -> PC7 (+ ~330R)
 *   LDR1 -> PA1 (ADC_IN1)         LDR2  -> PA2 (ADC_IN2)
 *   HC-SR04: TRIG -> PA9, ECHO -> PA8 (5V tolerant), VCC 5V
 *   MOTOR PWM -> PB4 (TIM3_CH1); servo signal or driver input
 *   BTN USER -> PA0, active high (short = motor +-10, long = AUX1)
 *
 * Backlight: if the module has no onboard driver transistor, do not feed
 * the LED pin from PB9 directly - use a small NPN/NMOS, or tie it to 3.3V
 * (always on, no fade/dimming).
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32f0xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ---- fast pin helpers ------------------------------------------------- */
#define PIN_HI(port, pin)   ((port)->BSRR = (pin))
#define PIN_LO(port, pin)   ((port)->BRR  = (pin))

#define LCD_CS_PORT   GPIOB
#define LCD_CS_PIN    GPIO_PIN_6
#define LCD_DC_PORT   GPIOB
#define LCD_DC_PIN    GPIO_PIN_7
#define LCD_RST_PORT  GPIOB
#define LCD_RST_PIN   GPIO_PIN_8

#define TP_CS_PORT    GPIOB
#define TP_CS_PIN     GPIO_PIN_12
#define TP_IRQ_PORT   GPIOB
#define TP_IRQ_PIN    GPIO_PIN_11

#define LED1_PORT     GPIOC
#define LED1_PIN      GPIO_PIN_8
#define LED2_PORT     GPIOC
#define LED2_PIN      GPIO_PIN_9
#define LEDG_PORT     GPIOC
#define LEDG_PIN      GPIO_PIN_6
#define LEDR_PORT     GPIOC
#define LEDR_PIN      GPIO_PIN_7

#define BTN_PORT      GPIOA
#define BTN_PIN       GPIO_PIN_0

/* LED index for board_led(): */
#define LED_AUX1   0     /* PC8  */
#define LED_AUX2   1     /* PC9  */
#define LED_GREEN  2     /* PC6  */
#define LED_RED    3     /* PC7  */

/* Motor output on PB4 (TIM3_CH1):
 * MOTOR_SERVO 1 -> 50 Hz RC-servo signal, motor_set(0..180) degrees
 * MOTOR_SERVO 0 -> 1 kHz PWM duty,        motor_set(0..100) percent   */
#ifndef MOTOR_SERVO
#define MOTOR_SERVO   1
#endif

void     board_clock48(void);          /* switch HSI8 -> PLL 48 MHz        */
void     board_init(void);             /* GPIO, TIM16 (us), TIM17 (PWM BL) */

void     board_led(uint8_t idx, bool on);   /* LED_AUX1..LED_RED           */
bool     board_led_get(uint8_t idx);
bool     board_btn(void);

void     motor_init(void);
void     motor_set(uint16_t v);        /* degrees (servo) or % (dc)        */                   /* true while pressed          */

void     board_backlight(uint16_t pct);     /* 0..100 %                    */
uint16_t board_backlight_get(void);

void     delay_us(uint32_t us);

/* Touch calibration persistence (last flash page, 0x0800FC00) */
typedef struct {
  uint32_t magic;
  int16_t  rx1, ry1;   /* raw readings at screen point 1                   */
  int16_t  rx2, ry2;   /* raw readings at screen point 2                   */
  uint8_t  swap;       /* raw axes swapped relative to screen              */
  uint8_t  pad[3];
} TouchCal;

bool board_cal_load(TouchCal *c);
bool board_cal_save(const TouchCal *c);

#endif
