#include <Arduino.h>
#include "Arduino_GFX_Library.h"
#include "image_data.h"
#include <WiFi.h>
#include <time.h>
// ST7789 TFTディスプレイピン定義
#define TFT_MOSI  6
#define TFT_SCLK  4
#define TFT_CS    7
#define TFT_RST   3
#define TFT_DC    2

// Arduino_GFX ディスプレイオブジェクト
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, -1);
Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, 1 /* rotation */, true /* IPS */, 170 /* width */, 320 /* height */, 35 /* col offset 1 */, 0 /* row offset 1 */, 35 /* col offset 2 */, 0 /* row offset 2 */);

// WiFi設定
const char *ssid = "MY-SSID";     // WiFi SSID を入力
const char *password = "MY-PASSWORD"; // WiFi パスワードを入力

// 時刻設定
const char *timeZone = "JST-9";
const char *server = "ntp.nict.jp";
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

// 超高速描画（Arduino_GFX一括転送）
void drawBufferedImage() {
  // Arduino_GFXの高速描画関数を使用
  gfx->draw16bitRGBBitmap(displayX, displayY, imageBuffer, scaledWidth, scaledHeight);
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

// ニキシー管数字表示関数
void drawNixieDigit(int16_t x, int16_t y, int digit, int16_t dw, int16_t dh) {
  const uint16_t* nixieImages[] = {
    H00, H10, H20, H30, H40, H50, H60, H70, H80, H90
  };
  
  if (digit >= 0 && digit <= 9) {
    scaleImage(nixieImages[digit], iw, ih, imageBuffer, dw, dh);
    gfx->draw16bitRGBBitmap(x, y, imageBuffer, dw, dh);
  }
}

// コロン表示関数
void drawColon(int16_t x, int16_t y, bool visible) {
  int16_t dotSize = 3;
  int16_t dotSpacing = 30;
  
  uint16_t color = visible ? 0xFEF0 : BLACK;
  gfx->fillCircle(x, y - dotSpacing/2, dotSize, color);
  gfx->fillCircle(x, y + dotSpacing/2, dotSize, color);
}

// 時計表示更新関数
void updateClock() {
  int h1 = hour / 10;
  int h0 = hour % 10;
  int m1 = minute / 10;
  int m0 = minute % 10;
  int s1 = second / 10;
  int s0 = second % 10;
  
  // 横向き表示用のレイアウト計算
  int16_t screenW = gfx->width();   // 横向きなので320
  int16_t screenH = gfx->height();  // 横向きなので170
  
  // 6桁 + コロン2つのレイアウト（横向きに最適化）
  int16_t digitW = screenW / 7;     // 余裕をもって8分割
  int16_t digitH = (screenH < ((digitW * ih) / iw)) ? screenH : ((digitW * ih) / iw);
  
  // 中央揃えで配置
  int16_t totalWidth = digitW * 6 + 30; // 6桁 + コロン2つ分
  int16_t startX = (screenW - totalWidth) / 2;
  int16_t startY = (screenH - digitH) / 2;
  
  // 時間表示（変更された桁のみ更新）
  if (bh1 != h1) {
    drawNixieDigit(startX, startY, h1, digitW, digitH);
    bh1 = h1;
  }
  if (bh0 != h0) {
    drawNixieDigit(startX + digitW, startY, h0, digitW, digitH);
    bh0 = h0;
  }
  
  // コロン1
  drawColon(startX + digitW * 2 + 5, startY + digitH/2, (second % 2 == 0));
  
  // 分表示
  if (bm1 != m1) {
    drawNixieDigit(startX + digitW * 2 + 10, startY, m1, digitW, digitH);
    bm1 = m1;
  }
  if (bm0 != m0) {
    drawNixieDigit(startX + digitW * 3 + 10, startY, m0, digitW, digitH);
    bm0 = m0;
  }
  
  // コロン2
  drawColon(startX + digitW * 4 + 15, startY + digitH/2, (second % 2 == 0));
  
  // 秒表示
  if (bs1 != s1) {
    drawNixieDigit(startX + digitW * 4 + 20, startY, s1, digitW, digitH);
    bs1 = s1;
  }
  if (bs0 != s0) {
    drawNixieDigit(startX + digitW * 5 + 20, startY, s0, digitW, digitH);
    bs0 = s0;
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
  Serial.println("Nixie Tube Clock Start");
  
  // Arduino_GFX ST7789ディスプレイ初期化
  gfx->begin();
  gfx->fillScreen(BLACK);
  
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