# 画面切り替え高速化メモ

## 現在の最適化 (完了)

- [x] 固定サイズバッファ使用
- [x] スケーリングアルゴリズム最適化
- [x] SPI 速度 80MHz
- [x] 事前計算の活用

## さらなる高速化の提案

### 1. Arduino_GFX_Library への移行

```cpp
// platformio.ini に追加
lib_deps =
    moononournation/GFX Library for Arduino@^1.3.7

// main.cpp の変更例
#include "Arduino_GFX_Library.h"
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, -1);
Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, 0, true, 170, 320);
```

### 2. DMA 転送の活用

ESP32 の DMA 機能を使用して SPI 転送を並列化

### 3. 画像データの最適化

- RGB565 形式の最適化
- PROGMEM 配置の確認

## 性能比較

- nixie_tube.ino: Arduino_GFX + 最適化アルゴリズム
- main.cpp (改良後): Adafruit_ST7789 + 最適化アルゴリズム

期待される改善: 約 30-50%の高速化
