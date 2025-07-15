#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include "image_data.h"  // 画像データヘッダーをインクルード

// ESP32-C3の内蔵LED
#define LED_PIN 8

// ST7789 TFTディスプレイピン定義
#define TFT_MOSI  6
#define TFT_SCLK  4
#define TFT_CS    7
#define TFT_RST   3
#define TFT_DC    2

// ST7789ディスプレイオブジェクト
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

void setup() {
  Serial.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);
  
  // ST7789ディスプレイ初期化
  tft.init(170, 320);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  
  // 画像を表示
  tft.drawRGBBitmap(10, 10, H00, H00_WIDTH, H00_HEIGHT);
  
  // テキスト表示
  tft.setCursor(120, 20);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.println("Image Test");
  
  Serial.println("Image displayed!");
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  
  // LED状態表示
  tft.fillRect(120, 50, 150, 30, ST77XX_BLACK);
  tft.setCursor(120, 50);
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(2);
  tft.println("LED: ON");
  
  delay(1000);
  
  digitalWrite(LED_PIN, LOW);
  
  tft.fillRect(120, 50, 150, 30, ST77XX_BLACK);
  tft.setCursor(120, 50);
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(2);
  tft.println("LED: OFF");
  
  delay(1000);
}