/* board.c - clock, GPIO, timers, backlight PWM, flash storage */
#include "board.h"
#include "main.h"

static uint16_t s_bl_pct = 100;

/* -------------------------------------------------- 48 MHz system clock */
void board_clock48(void)
{
  RCC_OscInitTypeDef osc = {0};
  RCC_ClkInitTypeDef clk = {0};

  osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
  osc.HSIState            = RCC_HSI_ON;
  osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  osc.PLL.PLLState        = RCC_PLL_ON;
  osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI;      /* HSI/2 = 4 MHz      */
  osc.PLL.PLLMUL          = RCC_PLL_MUL12;          /* x12   = 48 MHz     */
  if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

  clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
  clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  clk.APB1CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_1) != HAL_OK) Error_Handler();
}

/* ------------------------------------------------------------ GPIO etc. */
void board_init(void)
{
  GPIO_InitTypeDef io = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /* LCD control lines */
  PIN_HI(LCD_CS_PORT, LCD_CS_PIN);
  PIN_HI(LCD_DC_PORT, LCD_DC_PIN);
  PIN_HI(LCD_RST_PORT, LCD_RST_PIN);
  io.Mode  = GPIO_MODE_OUTPUT_PP;
  io.Pull  = GPIO_NOPULL;
  io.Speed = GPIO_SPEED_FREQ_HIGH;
  io.Pin   = LCD_CS_PIN | LCD_DC_PIN | LCD_RST_PIN;
  HAL_GPIO_Init(GPIOB, &io);

  /* Touch CS + IRQ */
  PIN_HI(TP_CS_PORT, TP_CS_PIN);
  io.Pin = TP_CS_PIN;
  HAL_GPIO_Init(TP_CS_PORT, &io);
  io.Mode = GPIO_MODE_INPUT;
  io.Pull = GPIO_PULLUP;
  io.Pin  = TP_IRQ_PIN;
  HAL_GPIO_Init(TP_IRQ_PORT, &io);

  /* LEDs: AUX1/AUX2 on PC8/PC9, status green/red on PC6/PC7 */
  PIN_LO(LED1_PORT, LED1_PIN);
  PIN_LO(LED2_PORT, LED2_PIN);
  PIN_LO(LEDG_PORT, LEDG_PIN);
  PIN_LO(LEDR_PORT, LEDR_PIN);
  io.Mode = GPIO_MODE_OUTPUT_PP;
  io.Pull = GPIO_NOPULL;
  io.Pin  = LED1_PIN | LED2_PIN | LEDG_PIN | LEDR_PIN;
  HAL_GPIO_Init(GPIOC, &io);

  /* User button, active high (internal pulldown keeps the pin defined
     on boards without the Discovery's external pulldown) */
  io.Mode = GPIO_MODE_INPUT;
  io.Pull = GPIO_PULLDOWN;
  io.Pin  = BTN_PIN;
  HAL_GPIO_Init(BTN_PORT, &io);

  /* TIM16: free-running 1 MHz for delay_us */
  __HAL_RCC_TIM16_CLK_ENABLE();
  TIM16->PSC = (SystemCoreClock / 1000000u) - 1u;
  TIM16->ARR = 0xFFFF;
  TIM16->EGR = TIM_EGR_UG;
  TIM16->CR1 = TIM_CR1_CEN;

  /* TIM17 CH1 on PB9 (AF2): 24 kHz backlight PWM */
  __HAL_RCC_TIM17_CLK_ENABLE();
  io.Mode      = GPIO_MODE_AF_PP;
  io.Pull      = GPIO_NOPULL;
  io.Speed     = GPIO_SPEED_FREQ_LOW;
  io.Alternate = GPIO_AF2_TIM17;
  io.Pin       = GPIO_PIN_9;
  HAL_GPIO_Init(GPIOB, &io);
  TIM17->PSC   = 1;
  TIM17->ARR   = 999;
  TIM17->CCR1  = 0;                       /* start dark, boot fades in    */
  TIM17->CCMR1 = (6u << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE;
  TIM17->CCER  = TIM_CCER_CC1E;
  TIM17->BDTR  = TIM_BDTR_MOE;
  TIM17->EGR   = TIM_EGR_UG;
  TIM17->CR1   = TIM_CR1_ARPE | TIM_CR1_CEN;
  s_bl_pct = 0;
}

void board_backlight(uint16_t pct)
{
  if (pct > 100) pct = 100;
  s_bl_pct = pct;
  /* perceptual-ish curve: pwm = pct^2 / 10 (0..1000) */
  TIM17->CCR1 = ((uint32_t)pct * pct) / 10u;
}

uint16_t board_backlight_get(void) { return s_bl_pct; }

static const uint16_t led_pin[4] = { LED1_PIN, LED2_PIN, LEDG_PIN, LEDR_PIN };

void board_led(uint8_t idx, bool on)
{
  if (idx > 3) return;
  if (on) PIN_HI(GPIOC, led_pin[idx]); else PIN_LO(GPIOC, led_pin[idx]);
}

bool board_led_get(uint8_t idx)
{
  if (idx > 3) return false;
  return (GPIOC->ODR & led_pin[idx]) != 0;
}

/* --------------------------------------------- motor PWM (TIM3 CH1) */
void motor_init(void)
{
  GPIO_InitTypeDef io = {0};

  __HAL_RCC_TIM3_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  io.Mode      = GPIO_MODE_AF_PP;
  io.Pull      = GPIO_NOPULL;
  io.Speed     = GPIO_SPEED_FREQ_LOW;
  io.Alternate = GPIO_AF1_TIM3;
  io.Pin       = GPIO_PIN_4;
  HAL_GPIO_Init(GPIOB, &io);

  TIM3->PSC   = (SystemCoreClock / 1000000u) - 1u;   /* 1 MHz base      */
#if MOTOR_SERVO
  TIM3->ARR   = 19999;                               /* 50 Hz           */
  TIM3->CCR1  = 1500;                                /* center          */
#else
  TIM3->ARR   = 999;                                 /* 1 kHz           */
  TIM3->CCR1  = 0;
#endif
  TIM3->CCMR1 = (6u << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE;
  TIM3->CCER  = TIM_CCER_CC1E;
  TIM3->EGR   = TIM_EGR_UG;
  TIM3->CR1   = TIM_CR1_ARPE | TIM_CR1_CEN;
}

void motor_set(uint16_t v)
{
#if MOTOR_SERVO
  if (v > 180) v = 180;
  TIM3->CCR1 = 500u + ((uint32_t)v * 2000u) / 180u;  /* 0.5..2.5 ms     */
#else
  if (v > 100) v = 100;
  TIM3->CCR1 = (uint32_t)v * 10u;
#endif
}

bool board_btn(void)
{
  return (BTN_PORT->IDR & BTN_PIN) != 0;
}

void delay_us(uint32_t us)
{
  while (us > 30000u) { delay_us(30000u); us -= 30000u; }
  uint16_t start = TIM16->CNT;
  while ((uint16_t)(TIM16->CNT - start) < (uint16_t)us) { }
}

/* --------------------------------------------- touch cal flash storage */
#define CAL_ADDR   0x0800FC00u          /* last 1 KB page (reserved in .ld) */
#define CAL_MAGIC  0x54435031u          /* "TCP1" */

bool board_cal_load(TouchCal *c)
{
  const TouchCal *f = (const TouchCal *)CAL_ADDR;
  if (f->magic != CAL_MAGIC) return false;
  *c = *f;
  return true;
}

bool board_cal_save(const TouchCal *c)
{
  TouchCal tmp = *c;
  tmp.magic = CAL_MAGIC;

  FLASH_EraseInitTypeDef er = {0};
  uint32_t pageErr = 0;
  er.TypeErase   = FLASH_TYPEERASE_PAGES;
  er.PageAddress = CAL_ADDR;
  er.NbPages     = 1;

  HAL_FLASH_Unlock();
  if (HAL_FLASHEx_Erase(&er, &pageErr) != HAL_OK) { HAL_FLASH_Lock(); return false; }

  const uint32_t *src = (const uint32_t *)&tmp;
  for (uint32_t i = 0; i < (sizeof(TouchCal) + 3u) / 4u; i++) {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, CAL_ADDR + 4u * i, src[i]) != HAL_OK) {
      HAL_FLASH_Lock();
      return false;
    }
  }
  HAL_FLASH_Lock();
  return true;
}
