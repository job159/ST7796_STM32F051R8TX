/* sensors.c - ADC (2x LDR) + HC-SR04, register level, non-blocking */
#include "sensors.h"

/* ------------------------------------------------------------- ADC -- */
static uint16_t adc_read(uint8_t ch)
{
  ADC1->CHSELR = 1u << ch;
  ADC1->ISR    = ADC_ISR_EOC | ADC_ISR_EOS;
  ADC1->CR    |= ADC_CR_ADSTART;
  while (!(ADC1->ISR & ADC_ISR_EOC)) { }
  return (uint16_t)ADC1->DR;
}

static void adc_init(void)
{
  GPIO_InitTypeDef io = {0};

  __HAL_RCC_ADC1_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  io.Mode = GPIO_MODE_ANALOG;
  io.Pull = GPIO_NOPULL;
  io.Pin  = GPIO_PIN_1 | GPIO_PIN_2;
  HAL_GPIO_Init(GPIOA, &io);

  ADC1->CFGR2 = ADC_CFGR2_CKMODE_1;          /* PCLK/4 = 12 MHz          */
  ADC1->SMPR  = 6;                           /* 71.5 cycles sample time  */

  /* calibrate, then enable */
  if (ADC1->CR & ADC_CR_ADEN) {
    ADC1->CR |= ADC_CR_ADDIS;
    while (ADC1->CR & ADC_CR_ADEN) { }
  }
  ADC1->CR |= ADC_CR_ADCAL;
  while (ADC1->CR & ADC_CR_ADCAL) { }
  delay_us(5);
  ADC1->ISR = ADC_ISR_ADRDY;
  ADC1->CR |= ADC_CR_ADEN;
  while (!(ADC1->ISR & ADC_ISR_ADRDY)) { }
}

static uint16_t s_light_raw[2];              /* EMA-filtered, q(12)      */
static const uint8_t s_light_ch[2] = { 1, 2 };   /* PA1, PA2             */

static void light_sample(void)
{
  for (int i = 0; i < 2; i++) {
    uint32_t acc = 0;
    for (int k = 0; k < 4; k++) acc += adc_read(s_light_ch[i]);
    uint16_t raw = (uint16_t)(acc / 4u);
    /* EMA 1/4 */
    s_light_raw[i] = (uint16_t)((3u * s_light_raw[i] + raw) / 4u);
  }
}

LightVal light_get(uint8_t idx)
{
  LightVal v;
  uint32_t raw = s_light_raw[idx & 1];
  v.mv  = (uint16_t)((raw * 3300u) / 4095u);
  v.pct = (uint8_t)((raw * 100u) / 4095u);
  v.open = (raw < 25u) || (raw > 4070u);
#if LDR_ON_TOP
  v.ohm = raw ? (LDR_RFIX * (4095u - raw)) / raw : 9999999u;
#else
  v.ohm = (raw < 4095u) ? (LDR_RFIX * raw) / (4095u - raw) : 9999999u;
#endif
  if (v.ohm > 9999999u) v.ohm = 9999999u;
  return v;
}

/* --------------------------------------------------------- HC-SR04 -- */
#define TRIG_PORT GPIOA
#define TRIG_PIN  GPIO_PIN_9
#define ECHO_PORT GPIOA
#define ECHO_PIN  GPIO_PIN_8

#define PING_PERIOD_MS  80u
#define PING_TIMEOUT_MS 40u

static volatile uint16_t s_echo_t0;
static volatile uint16_t s_echo_us;
static volatile uint8_t  s_echo_done;        /* 0 idle, 1 armed, 2 done  */

static uint32_t s_ping_t;                    /* last trigger tick        */
static uint16_t s_med[5];                    /* raw mm ring for median   */
static uint8_t  s_med_n, s_med_i;
static uint8_t  s_miss;
static DistVal  s_dist = { 0xFFFF, DIST_INVALID };

void hcsr04_echo_isr(void)
{
  uint16_t now = (uint16_t)TIM16->CNT;
  if (ECHO_PORT->IDR & ECHO_PIN) {
    s_echo_t0 = now;
  } else if (s_echo_done == 1) {
    s_echo_us   = (uint16_t)(now - s_echo_t0);
    s_echo_done = 2;
  }
}

static void hcsr04_init(void)
{
  GPIO_InitTypeDef io = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_SYSCFG_CLK_ENABLE();

  PIN_LO(TRIG_PORT, TRIG_PIN);
  io.Mode  = GPIO_MODE_OUTPUT_PP;
  io.Pull  = GPIO_NOPULL;
  io.Speed = GPIO_SPEED_FREQ_LOW;
  io.Pin   = TRIG_PIN;
  HAL_GPIO_Init(TRIG_PORT, &io);

  io.Mode = GPIO_MODE_INPUT;                 /* ECHO: PA8 is 5V tolerant */
  io.Pull = GPIO_PULLDOWN;
  io.Pin  = ECHO_PIN;
  HAL_GPIO_Init(ECHO_PORT, &io);

  /* EXTI8 <- PA8, both edges */
  SYSCFG->EXTICR[2] &= ~SYSCFG_EXTICR3_EXTI8;
  EXTI->IMR  |= EXTI_IMR_MR8;
  EXTI->RTSR |= EXTI_RTSR_TR8;
  EXTI->FTSR |= EXTI_FTSR_TR8;
  NVIC_SetPriority(EXTI4_15_IRQn, 1);
  NVIC_EnableIRQ(EXTI4_15_IRQn);
}

static uint16_t median5(void)
{
  uint16_t v[5];
  uint8_t n = s_med_n;
  for (int i = 0; i < n; i++) v[i] = s_med[i];
  for (int i = 1; i < n; i++) {
    uint16_t k = v[i]; int j = i - 1;
    while (j >= 0 && v[j] > k) { v[j + 1] = v[j]; j--; }
    v[j + 1] = k;
  }
  return v[n / 2];
}

static void dist_update(uint16_t mm_raw, bool got)
{
  if (got) {
    s_miss = 0;
    s_med[s_med_i] = mm_raw;
    s_med_i = (uint8_t)((s_med_i + 1) % 5);
    if (s_med_n < 5) s_med_n++;
    uint16_t med = median5();
    /* EMA 1/2 on the median */
    s_dist.mm = (s_dist.mm == 0xFFFF) ? med
              : (uint16_t)((s_dist.mm + med) / 2u);
  } else {
    if (s_miss < 200) s_miss++;
    if (s_miss >= 3) { s_med_n = 0; s_med_i = 0; s_dist.mm = 0xFFFF; }
    if (s_miss < 3) return;                  /* keep last state briefly  */
  }

  if (s_dist.mm == 0xFFFF ||
      s_dist.mm > DIST_MAX_MM || s_dist.mm < DIST_MIN_MM)
    s_dist.state = DIST_INVALID;
  else if (s_dist.mm < DIST_NEAR_MM)
    s_dist.state = DIST_NEAR;
  else
    s_dist.state = DIST_VALID;
}

DistVal dist_get(void) { return s_dist; }

void sensors_init(void)
{
  adc_init();
  hcsr04_init();
  s_light_raw[0] = adc_read(s_light_ch[0]);
  s_light_raw[1] = adc_read(s_light_ch[1]);
  s_ping_t = HAL_GetTick();
}

void sensors_task(void)
{
  uint32_t now = HAL_GetTick();

  light_sample();

  if (s_echo_done == 2) {                    /* echo captured            */
    uint32_t us = s_echo_us;
    s_echo_done = 0;
    /* 0.1715 mm per us (round trip at 343 m/s) */
    uint32_t mm = (us * 1715u) / 10000u;
    bool sane = (us > 80u) && (us < 30000u);
    dist_update(sane ? (uint16_t)mm : 0, sane);
  } else if (s_echo_done == 1 &&
             now - s_ping_t > PING_TIMEOUT_MS) {
    s_echo_done = 0;                         /* no echo                  */
    dist_update(0, false);
  }

  if (s_echo_done == 0 && now - s_ping_t >= PING_PERIOD_MS) {
    s_ping_t = now;
    s_echo_done = 1;
    PIN_HI(TRIG_PORT, TRIG_PIN);
    delay_us(10);
    PIN_LO(TRIG_PORT, TRIG_PIN);
  }
}
