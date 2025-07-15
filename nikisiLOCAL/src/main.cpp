#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include "image_data.h"  // 画像データヘッダーをインクルード

// ST7789 TFTディスプレイピン定義
#define TFT_MOSI  6
#define TFT_SCLK  4
#define TFT_CS    7
#define TFT_RST   3
#define TFT_DC    2

// ST7789ディスプレイオブジェクト
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// より高速な拡大表示関数
void drawScaledBitmapFast(int16_t x, int16_t y, const uint16_t* bitmap, int16_t w, int16_t h, int16_t scale) {
  for (int16_t i = 0; i < h; i++) {
    for (int16_t sy = 0; sy < scale; sy++) {
      for (int16_t j = 0; j < w; j++) {
        uint16_t pixel = pgm_read_word(&bitmap[i * w + j]);
        // 水平ラインで描画（高速化）
        tft.drawFastHLine(x + j * scale, y + i * scale + sy, scale, pixel);
      }
    }
  }
}

void drawImageFitScreen(const uint16_t* bitmap, int16_t w, int16_t h) {
  // 画面サイズを取得（回転後）
  int16_t screenW = tft.width();
  int16_t screenH = tft.height();
  
  // 拡大倍率を計算（画面に収まる最大倍率）
  int16_t scaleX = screenW / w;
  int16_t scaleY = screenH / h;
  int16_t scale = min(scaleX, scaleY);
  
  if (scale < 1) scale = 1;  // 最小倍率は1倍
  
  // 中央配置の計算
  int16_t x = (screenW - w * scale) / 2;
  int16_t y = (screenH - h * scale) / 2;
  
  Serial.print("Scale: ");
  Serial.print(scale);
  Serial.print(", Position: (");
  Serial.print(x);
  Serial.print(", ");
  Serial.print(y);
  Serial.println(")");
  
  // 拡大画像を描画
  drawScaledBitmapFast(x, y, bitmap, w, h, scale);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Image Display Start");
  
  // ST7789ディスプレイ初期化
  tft.init(170, 320);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  
  // 画面サイズに合わせて自動拡大
  drawImageFitScreen(H00, H00_WIDTH, H00_HEIGHT);
  
  Serial.println("Auto-scaled image displayed!");
}

void loop() {
  // 何もしない（画像表示のみ）
  delay(1000);
}