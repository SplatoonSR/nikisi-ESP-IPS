#include <Arduino.h>
#include "Arduino_GFX_Library.h"
#include "image_data.h"
#include <WiFi.h>
#include <time.h>
#include "config.h"  // WiFi設定をインクルード　*ssid　*password　*server　*timeZoneを外部で定義

// ST7789 TFTディスプレイピン定義
#define TFT_MOSI  6
#define TFT_SCLK  4
#define TFT_RST   -1
#define TFT_DC    2

#define TFT_CS_1  7 //ディスプレイ1 (秒一の位)
#define TFT_CS_2  8 //ディスプレイ2 (秒十の位)
#define TFT_CS_3  9 //ディスプレイ3 (分一の位)
#define TFT_CS_4  10 //ディスプレイ4 (分十の位)
#define TFT_CS_5  20 //ディスプレイ5 (時一の位)
#define TFT_CS_6  21 //ディスプレイ6 (時十の位)



// Arduino_GFX ディスプレイオブジェクト（6個）
Arduino_DataBus *bus1 = new Arduino_ESP32SPI(TFT_DC, TFT_CS_1, TFT_SCLK, TFT_MOSI, -1);
Arduino_DataBus *bus2 = new Arduino_ESP32SPI(TFT_DC, TFT_CS_2, TFT_SCLK, TFT_MOSI, -1);
Arduino_DataBus *bus3 = new Arduino_ESP32SPI(TFT_DC, TFT_CS_3, TFT_SCLK, TFT_MOSI, -1);
Arduino_DataBus *bus4 = new Arduino_ESP32SPI(TFT_DC, TFT_CS_4, TFT_SCLK, TFT_MOSI, -1);
Arduino_DataBus *bus5 = new Arduino_ESP32SPI(TFT_DC, TFT_CS_5, TFT_SCLK, TFT_MOSI, -1);
Arduino_DataBus *bus6 = new Arduino_ESP32SPI(TFT_DC, TFT_CS_6, TFT_SCLK, TFT_MOSI, -1);

Arduino_GFX *gfx1 = new Arduino_ST7789(bus1, TFT_RST, 2, true, 170, 320, 35, 0, 35, 0); // 秒一の位
Arduino_GFX *gfx2 = new Arduino_ST7789(bus2, TFT_RST, 2, true, 170, 320, 35, 0, 35, 0); // 秒十の位
Arduino_GFX *gfx3 = new Arduino_ST7789(bus3, TFT_RST, 2, true, 170, 320, 35, 0, 35, 0); // 分一の位
Arduino_GFX *gfx4 = new Arduino_ST7789(bus4, TFT_RST, 2, true, 170, 320, 35, 0, 35, 0); // 分十の位
Arduino_GFX *gfx5 = new Arduino_ST7789(bus5, TFT_RST, 2, true, 170, 320, 35, 0, 35, 0); // 時一の位
Arduino_GFX *gfx6 = new Arduino_ST7789(bus6, TFT_RST, 2, true, 170, 320, 35, 0, 35, 0); // 時十の位

// ディスプレイ配列（6個）
Arduino_GFX* displays[6] = {gfx1, gfx2, gfx3, gfx4, gfx5, gfx6};

struct tm localTime;

// 時計変数
int hour = 0, minute = 0, second = 0;
int bh1 = 99, bh0 = 99, bm1 = 99, bm0 = 99, bs1 = 99, bs0 = 99; // 前回の時刻
unsigned long ps_Time = 0;

// ニキシー管画像サイズ
int iw = 70;   // 幅
int ih = 134;  // 高さ

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

// スケーリング関数（nixie_tube.inoと同じ方式）
void scaleImage(const uint16_t* src, int w1, int h1, uint16_t* dst, int w2, int h2) {
    for (int y = 0; y < h2; y++) {
        int srcY = (y * h1) / h2; // 縦方向のスケーリング
        for (int x = 0; x < w2; x++) {
            int srcX = (x * w1) / w2; // 横方向のスケーリング
            dst[y * w2 + x] = pgm_read_word(&src[srcY * w1 + srcX]);
        }
    }
}

// ニキシー管数字表示関数（指定ディスプレイに表示）
void drawNixieDigitOnDisplay(Arduino_GFX* display, int digit) {
  const uint16_t* nixieImages[] = {
    H00, H10, H20, H30, H40, H50, H60, H70, H80, H90
  };
  
  if (digit >= 0 && digit <= 9) {
    // ディスプレイ全体にニキシー管画像を表示
    int16_t screenW = display->width();
    int16_t screenH = display->height();
    
    // 画像をディスプレイサイズにスケーリング
    scaleImage(nixieImages[digit], iw, ih, imageBuffer, screenW, screenH);
    display->draw16bitRGBBitmap(0, 0, imageBuffer, screenW, screenH);
  }
}

// ニキシー管数字表示関数（旧版 - 一つのディスプレイ用）
void drawNixieDigit(int16_t x, int16_t y, int digit, int16_t dw, int16_t dh) {
  const uint16_t* nixieImages[] = {
    H00, H10, H20, H30, H40, H50, H60, H70, H80, H90
  };
  
  if (digit >= 0 && digit <= 9) {
    scaleImage(nixieImages[digit], iw, ih, imageBuffer, dw, dh);
    gfx1->draw16bitRGBBitmap(x, y, imageBuffer, dw, dh);
  }
}

// コロン表示関数
void drawColon(int16_t x, int16_t y, bool visible) {
  int16_t dotSize = 3;
  int16_t dotSpacing = 30;
  
  uint16_t color = visible ? 0xFEF0 : BLACK;
  gfx1->fillCircle(x, y - dotSpacing/2, dotSize, color);
  gfx1->fillCircle(x, y + dotSpacing/2, dotSize, color);
}

// 時計表示更新関数（6個のディスプレイ用）
void updateClock() {
  int h1 = hour / 10;   // 時十の位
  int h0 = hour % 10;   // 時一の位
  int m1 = minute / 10; // 分十の位
  int m0 = minute % 10; // 分一の位
  int s1 = second / 10; // 秒十の位
  int s0 = second % 10; // 秒一の位
  
  // HH:MM:SS形式で表示（右から左の順番で修正）
  if (bs0 != s0) {
    drawNixieDigitOnDisplay(gfx1, s0); // ディスプレイ1: 秒一の位
    bs0 = s0;
    // Serial.println("Display 1 updated: " + String(s0));
  }
  if (bs1 != s1) {
    drawNixieDigitOnDisplay(gfx2, s1); // ディスプレイ2: 秒十の位
    bs1 = s1;
    // Serial.println("Display 2 updated: " + String(s1));
  }
  if (bm0 != m0) {
    drawNixieDigitOnDisplay(gfx3, m0); // ディスプレイ3: 分一の位
    bm0 = m0;
    // Serial.println("Display 3 updated: " + String(m0));
  }
  if (bm1 != m1) {
    drawNixieDigitOnDisplay(gfx4, m1); // ディスプレイ4: 分十の位
    bm1 = m1;
    Serial.println("Display 4 updated: " + String(m1));
  }
  if (bh0 != h0) {
    drawNixieDigitOnDisplay(gfx5, h0); // ディスプレイ5: 時一の位
    bh0 = h0;
    // Serial.println("Display 5 updated: " + String(h0));
  }
  if (bh1 != h1) {
    drawNixieDigitOnDisplay(gfx6, h1); // ディスプレイ6: 時十の位
    bh1 = h1;
    // Serial.println("Display 6 updated: " + String(h1));
  }
}

// WiFi接続関数
void initializeWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("WiFi connecting");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // NTP時刻同期
  configTime(9 * 3600, 0, server); // JST (UTC+9)
}

void setup() {
  Serial.begin(115200);
  Serial.println("Nixie Tube Clock Start (4 Display Test)");
  
  pinMode(TFT_RST, OUTPUT);
  pinMode(TFT_CS_1, OUTPUT);
  pinMode(TFT_CS_2, OUTPUT);
  pinMode(TFT_CS_3, OUTPUT);
  pinMode(TFT_CS_4, OUTPUT);
  pinMode(TFT_CS_5, OUTPUT);
  pinMode(TFT_CS_6, OUTPUT);
  digitalWrite(TFT_RST, HIGH); //RESET off
  digitalWrite(TFT_CS_1, HIGH); //CS off
  digitalWrite(TFT_CS_2, HIGH); //CS off
  digitalWrite(TFT_CS_3, HIGH); //CS off
  digitalWrite(TFT_CS_4, HIGH); //CS off
  digitalWrite(TFT_CS_5, HIGH); //CS off
  digitalWrite(TFT_CS_6, HIGH); //CS off
  delay(100); // 少し待つ
  // 4個のディスプレイを初期化
  for (int i = 0; i < 6; i++) {
    displays[i]->begin(10000000);
    // displays[i]->fillScreen(BLACK);
    // Serial.println("Display " + String(i+1) + " initialized");
  }
  delay(1000);
    for (int i = 0; i < 6; i++) {
    displays[i]->begin(10000000);
    // displays[i]->fillScreen(BLACK);
    // Serial.println("Display " + String(i+1) + " initialized");
  }
  

  // WiFi接続
  initializeWiFi();
  
  Serial.println("Nixie Tube Clock Ready!");
}

void loop() {
  // WiFi接続チェック
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    initializeWiFi();
  }
  
  // 1秒ごとに時刻更新
  if (millis() - ps_Time >= 1000) {
    if (getLocalTime(&localTime)) {
      hour = localTime.tm_hour;
      minute = localTime.tm_min;
      second = localTime.tm_sec;
      
      Serial.printf("%02d:%02d:%02d\n", hour, minute, second);
            // ニキシー管時計表示更新
      updateClock();
    } else {
      Serial.println("Failed to get time");
    }
    
    ps_Time = millis();
  }
  
  delay(50); // CPU負荷軽減
}