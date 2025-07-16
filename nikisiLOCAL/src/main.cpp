#include <Arduino.h>
#include "Arduino_GFX_Library.h"
#include "image_data.h"

// ST7789 TFTディスプレイピン定義
#define TFT_MOSI  6
#define TFT_SCLK  4
#define TFT_CS    7
#define TFT_RST   3
#define TFT_DC    2

// Arduino_GFX ディスプレイオブジェクト
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, -1);
Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, 2 /* rotation */, true /* IPS */, 170 /* width */, 320 /* height */, 35 /* col offset 1 */, 0 /* row offset 1 */, 35 /* col offset 2 */, 0 /* row offset 2 */);

// 高速化のためのバッファ（固定サイズで事前確保）
uint16_t imageBuffer[170*320] = {0}; // 最大画面サイズで固定確保
int16_t scaledWidth, scaledHeight;
int16_t displayX, displayY;

// 高速スケーリング関数（Arduino_GFX最適化版）
void scaleImageToBuffer(const uint16_t* bitmap, int16_t w, int16_t h, int16_t scale) {
  scaledWidth = w * scale;
  scaledHeight = h * scale;
  
  // 高速スケーリング（nixie_tube方式を採用）
  for (int16_t y = 0; y < scaledHeight; y++) {
    int16_t srcY = (y * h) / scaledHeight;
    for (int16_t x = 0; x < scaledWidth; x++) {
      int16_t srcX = (x * w) / scaledWidth;
      imageBuffer[y * scaledWidth + x] = pgm_read_word(&bitmap[srcY * w + srcX]);
    }
  }
}

// 超高速描画（Arduino_GFX一括転送）
void drawBufferedImage() {
  // Arduino_GFXの高速描画関数を使用
  gfx->draw16bitRGBBitmap(displayX, displayY, imageBuffer, scaledWidth, scaledHeight);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Arduino_GFX High-Speed Image Display Start");
  
  // Arduino_GFX ST7789ディスプレイ初期化
  gfx->begin();
  gfx->fillScreen(BLACK);
  
  // 拡大倍率と表示位置を事前計算
  int16_t screenW = gfx->width();
  int16_t screenH = gfx->height();
  int16_t scaleX = screenW / H00_WIDTH;
  int16_t scaleY = screenH / H00_HEIGHT;
  int16_t scale = min(scaleX, scaleY);
  if (scale < 1) scale = 1;
  
  displayX = (screenW - H00_WIDTH * scale) / 2;
  displayY = (screenH - H00_HEIGHT * scale) / 2;
  
  Serial.print("Scale: ");
  Serial.print(scale);
  Serial.print(", Buffer size: ");
  Serial.print((H00_WIDTH * scale * H00_HEIGHT * scale * 2) / 1024);
  Serial.println(" KB");
  
  // テスト表示
  gfx->fillRect(10, 10, 50, 50, RED);
  delay(1000);
  gfx->fillScreen(BLACK);
  
  Serial.println("Ready for high-speed animation!");
}

  // 画像配列定義
  const uint16_t* images[] = {
    H00, H10, H20, H30, H40, H50, H60, H70, H80, H90
  };
  const int imageCount = 10;

void loop() {
  // 拡大倍率を事前計算（setup()で計算済みの値を使用）
  static bool initialized = false;
  static int16_t scale;
  
  if (!initialized) {
    int16_t screenW = gfx->width();
    int16_t screenH = gfx->height();
    int16_t scaleX = screenW / H00_WIDTH;
    int16_t scaleY = screenH / H00_HEIGHT;
    scale = min(scaleX, scaleY);
    if (scale < 1) scale = 1;
    initialized = true;
  }
  
  for (int i = 0; i < imageCount; i++) {
    // 高速スケーリング（事前確保バッファ使用）
    scaleImageToBuffer(images[i], H00_WIDTH, H00_HEIGHT, scale);
    
    // Arduino_GFX 瞬時描画
    drawBufferedImage();
    
    Serial.print("Image ");
    Serial.print(i);
    Serial.println(" displayed");
    
    // 待機時間短縮
    delay(1000);
  }
}