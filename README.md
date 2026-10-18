# NEXUS Sensor Console — STM32F051 + 3.5" TFT 感測工作站

STM32F051R8T6(48 MHz、64KB Flash、8KB RAM)+ 3.5 吋 320×480 SPI TFT
(ST7796 或 ILI9488),整合真實感測與控制:

- **兩路光敏電阻**(ADC):即時顯示 **阻值 kΩ + 電壓 V** + 亮度條
- **HC-SR04 超音波測距**:中值 + EMA 濾波、非阻塞(EXTI 中斷計時),
  **有效窗 2–60 cm**,畫面顯示距離 / VALID / NEAR / TOO CLOSE / NO TARGET
- **狀態 LED**:有效 → 綠燈;無效(超程/過近/無回波)→ 紅燈;
  **距離 < 10 cm → 綠燈 250ms 閃爍**(畫面指示燈同步)
- **PWM 馬達**(預設伺服):**USER 鍵每按 +10°,到頂/到底自動反向**,
  畫面顯示角度、方向箭頭與 18 段位置條(每段 10°)
- 開機動畫、載入動畫、距離趨勢圖、狀態列時鐘 —— 延續 NEXUS 深色工控風格

全部圖形即時繪製(無 framebuffer):抗鋸齒圓角/圓形/漸層 +
Segoe UI Semibold 2bpp 抗鋸齒字型(含 **Ω、°** 符號)。

**資源:Flash 23.7KB / 63KB,靜態 RAM 3.3KB / 8KB。零編譯警告。**
畫面截圖(韌體繪圖程式碼實際輸出)見 `Docs/`。

---

## 接線總表

### 顯示器(ST7796 9-pin SPI 模組,`LCD_ST7796 = 1` 預設)

| 模組腳位 | STM32 | 模組腳位 | STM32 |
|---|---|---|---|
| GND | GND | RST | **PB8** |
| VCC | 3.3V | DC | **PB7** |
| SCL | **PA5** | CS | **PB6** |
| SDA | **PA7** | BL | **PB9**(PWM)或 3.3V |
| SDA-O | 不接 | | |

IM 跳線須為 4 線 SPI:**IM0=1、IM1=1、IM2=1**。
(ILI9488+XPT2046 觸控模組:`LCD_ST7796` 改 0,觸控接 PB11–PB15,見 board.h。)

### 感測與輸出

| 功能 | STM32 | 說明 |
|---|---|---|
| 光敏 1 | **PA1** (ADC_IN1) | 分壓中點,見下方電路 |
| 光敏 2 | **PA2** (ADC_IN2) | 同上 |
| HC-SR04 TRIG | **PA9** | 3.3V 準位即可觸發 |
| HC-SR04 ECHO | **PA8** | 5V 容忍腳,可直接接 |
| HC-SR04 VCC/GND | 5V / GND | 必須共地 |
| 綠色 LED | **PC6** | 串 ~330Ω 到 LED 到 GND |
| 紅色 LED | **PC7** | 同上 |
| 馬達 PWM | **PB4** (TIM3_CH1) | 伺服信號線,或驅動板 PWM 輸入 |
| USER 按鈕 | PA0 | 短按 = 馬達 ±10°,長按 = AUX1 |
| AUX LED | PC8 / PC9 | Discovery 板載 |

### 光敏分壓電路(固定電阻 1073Ω)

```
3.3V ──[光敏電阻]──●──[1073Ω]── GND
                   │
                  PA1 (PA2 同接法)
```

計算式:`R_LDR = 1073 × (4095 − ADC) / ADC`,亮 → 阻值小 → 電壓高。
若你把 1073Ω 接在上、光敏在下,把 `sensors.h` 的 `LDR_ON_TOP` 改 0 即可。

### 伺服馬達(SG90 類)

橙(信號)→ PB4,紅 → 5V,棕 → GND(與 MCU 共地)。
若是**直流馬達 + 驅動板**:`board.h` 的 `MOTOR_SERVO` 改 0,
變成 1 kHz PWM、按鍵每按 ±10% 佔空比,畫面自動改顯示 %。

---

## 操作

| 動作 | 功能 |
|---|---|
| 短按 USER | 馬達 +10°;到 180° 後自動改為每按 −10°,到 0° 再反向(過邊反轉) |
| 長按 USER(0.6s) | 切換 AUX1 LED(PC8),SYSTEM 卡指示燈同步 |
| 開機時按住 USER | 進入觸控三點校正(僅接觸控模組時有意義) |

### 距離狀態邏輯

| 條件 | 畫面 | 綠 LED (PC6) | 紅 LED (PC7) |
|---|---|---|---|
| 10–60 cm | VALID(綠) | 恆亮 | 滅 |
| 2–10 cm | NEAR(琥珀) | **250ms 閃爍** | 滅 |
| > 60 cm / 無回波 | NO TARGET(紅) | 滅 | 恆亮 |
| < 2 cm | TOO CLOSE(紅) | 滅 | 恆亮 |

濾波:每 80ms 測距一次 → 5 點中值 → EMA(½);連續 3 次無效才判失效,
不會因單次雜訊跳動。

---

## 建置

STM32CubeIDE 開啟專案直接 Build + 燒錄(Debug `-Og`、Release `-Os`)。
`.ioc` 不需配置 —— 全部週邊由程式碼初始化(USER CODE 區塊 + 自建檔案),
CubeMX 重新產生程式碼不會破壞。上電後 PC8+PC9(+PB9 背光)先閃 3 下
(心跳,證明韌體在跑)再進開機動畫。

## 常用調整

| 要改什麼 | 位置 |
|---|---|
| 驅動 IC(ST7796 ↔ ILI9488)| `Core/Inc/ili9488.h` → `LCD_ST7796` |
| 伺服 ↔ 直流馬達 PWM | `Core/Inc/board.h` → `MOTOR_SERVO`(1 伺服 / 0 直流)|
| 分壓上下顛倒 | `Core/Inc/sensors.h` → `LDR_ON_TOP` |
| 固定電阻值 | `sensors.h` → `LDR_RFIX`(目前 1073)|
| 距離門檻(2 / 10 / 60 cm)| `sensors.h` → `DIST_MIN_MM / DIST_NEAR_MM / DIST_MAX_MM` |
| 畫面旋轉 180° | `ili9488.h` → `LCD_MADCTL` 改 `0xE8` |
| 紅藍對調 / 反相 | `LCD_MADCTL` 去 BGR 位 / `LCD_INVERT` |
| SPI 速度(花屏時降速)| `Core/Src/ili9488.c` → `LCD_SPI_BR`(0=24MHz、1=12MHz)|
| 主題色 | `Core/Src/ui.c` 開頭 `C_*` |

## 架構

```
Core/Src
├─ board.c    48MHz 時脈、GPIO、TIM16(µs)、TIM17(背光)、TIM3(馬達PWM)、Flash 儲存
├─ ili9488.c  SPI1 暫存器直驅 24MHz,ST7796/ILI9488 初始化
├─ xpt2046.c  觸控(接觸控模組時用)
├─ sensors.c  ADC1 雙通道光敏 + HC-SR04(EXTI8 中斷計時、中值+EMA)
├─ gfx.c      抗鋸齒繪圖引擎(免 framebuffer)
├─ fonts.c    2bpp AA 字型(含 Ω、°),gen_fonts.ps1 可重生
└─ ui.c       開機/載入動畫、感測儀表板、LED/馬達邏輯、排程迴圈
```
