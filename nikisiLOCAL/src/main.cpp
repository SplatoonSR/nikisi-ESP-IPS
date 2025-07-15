#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include "image_data.h"

// ST7789 TFTディスプレイピン定義
#define TFT_MOSI  6
#define TFT_SCLK  4
#define TFT_CS    7
#define TFT_RST   3
#define TFT_DC    2

// ST7789ディスプレイオブジェクト
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// 高速化のためのバッファ
uint16_t* imageBuffer = nullptr;
int16_t scaledWidth, scaledHeight;
int16_t displayX, displayY;

// 高速スケーリング関数（ニキシー管方式）
void scaleImageToBuffer(const uint16_t* bitmap, int16_t w, int16_t h, int16_t scale) {
  scaledWidth = w * scale;
  scaledHeight = h * scale;
  
  // バッファが未確保または既存のバッファを再利用
  if (imageBuffer == nullptr) {
    imageBuffer = (uint16_t*)malloc(scaledWidth * scaledHeight * sizeof(uint16_t));
    if (imageBuffer == nullptr) {
      Serial.println("Memory allocation failed!");
      return;
    }
  }
  
  // 高速スケーリング（行列処理）
  for (int16_t y = 0; y < h; y++) {
    for (int16_t x = 0; x < w; x++) {
      uint16_t pixel = pgm_read_word(&bitmap[y * w + x]);
      
      // 拡大ブロックを一度に処理
      for (int16_t sy = 0; sy < scale; sy++) {
        uint16_t* bufferRow = &imageBuffer[(y * scale + sy) * scaledWidth + x * scale];
        for (int16_t sx = 0; sx < scale; sx++) {
          bufferRow[sx] = pixel;
        }
      }
    }
  }
}

// 超高速描画（一括転送）
void drawBufferedImage() {
  if (imageBuffer == nullptr) return;
  
  // 一括転送で瞬時に描画
  tft.drawRGBBitmap(displayX, displayY, imageBuffer, scaledWidth, scaledHeight);
}

void setup() {
  Serial.begin(115200);
  Serial.println("High-Speed Image Display Start");
  
  // ST7789ディスプレイ初期化
  tft.init(170, 320);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  
  // 拡大倍率と表示位置を事前計算
  int16_t screenW = tft.width();
  int16_t screenH = tft.height();
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
  tft.fillRect(10, 10, 50, 50, ST77XX_RED);
  delay(1000);
  tft.fillScreen(ST77XX_BLACK);
  
  Serial.println("Ready for high-speed animation!");
}

void loop() {
  // 画像配列
  const uint16_t* images[] = {
    H00, H10, H20, H30, H40, H50, H60, H70, H80, H90
  };
  
  const int imageCount = 10;
  
  // 拡大倍率を計算
  int16_t screenW = tft.width();
  int16_t screenH = tft.height();
  int16_t scaleX = screenW / H00_WIDTH;
  int16_t scaleY = screenH / H00_HEIGHT;
  int16_t scale = min(scaleX, scaleY);
  if (scale < 1) scale = 1;
  
  for (int i = 0; i < imageCount; i++) {
    // 画面クリア
    // tft.fillScreen(ST77XX_BLACK);
    
    // 高速スケーリング
    scaleImageToBuffer(images[i], H00_WIDTH, H00_HEIGHT, scale);
    
    // 瞬時描画
    drawBufferedImage();
    
    Serial.print("Image ");
    Serial.print(i);
    Serial.println(" displayed");
    
    // 1秒待機
    delay(1000);
  }
}