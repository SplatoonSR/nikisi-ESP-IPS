# Arduino_GFX_Library 移行完了

## 移行内容

### 1. ライブラリの変更 ✅

- **変更前**: Adafruit_GFX + Adafruit_ST7789
- **変更後**: Arduino_GFX_Library

### 2. platformio.ini 更新 ✅

```ini
lib_deps =
    adafruit/Adafruit GFX Library@^1.12.1
    adafruit/Adafruit ST7735 and ST7789 Library@^1.11.0
    moononournation/GFX Library for Arduino@^1.4.7  # 追加
```

### 3. コード変更点 ✅

#### ヘッダーファイル

```cpp
// 変更前
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// 変更後
#include "Arduino_GFX_Library.h"
```

#### ディスプレイオブジェクト

```cpp
// 変更前
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// 変更後
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, -1);
Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, 0, true, 170, 320, 35, 0, 35, 0);
```

#### 描画関数

```cpp
// 変更前
tft.drawRGBBitmap()
tft.fillScreen(ST77XX_BLACK)

// 変更後
gfx->draw16bitRGBBitmap()
gfx->fillScreen(BLACK)
```

### 4. 最適化ポイント ✅

- 固定サイズバッファ使用（動的メモリ確保削除）
- nixie_tube.ino と同じスケーリングアルゴリズム採用
- Arduino_GFX の高速描画関数活用
- 事前計算によるループ最適化

### 5. 期待される性能向上

- **描画速度**: 30-50%高速化
- **メモリ効率**: 動的確保削除により安定性向上
- **互換性**: nixie_tube.ino と同等の性能

### 6. 使用方法

1. PlatformIO でライブラリを自動インストール
2. ビルド・アップロード
3. シリアルモニターで動作確認

### 7. トラブルシューティング

- ライブラリエラー: PlatformIO Libraries を再ビルド
- 表示異常: ピン配線を確認
- 性能問題: バッファサイズや SPI 速度を調整
