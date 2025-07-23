#include <Arduino.h>
#include "Arduino_GFX_Library.h"
#include "image_data.h"
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
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

// Webサーバーオブジェクト
WebServer webServer(80);

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

// カスタム画像用バッファ（10個の数字分）
uint16_t* customImages[10] = {nullptr}; // カスタム画像ポインタ配列
bool useCustomImages = false; // カスタム画像を使用するかのフラグ
const int maxImageSize = 170 * 320; // 画像の最大サイズ（ディスプレイサイズ）
const int maxImageWidth = 170;  // 最大幅
const int maxImageHeight = 320; // 最大高さ

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

// カスタム画像をSPIFFSからロードする関数
bool loadCustomImage(int digit) {
  String filename = "/custom_" + String(digit) + ".rgb565";
  
  if (!SPIFFS.exists(filename)) {
    Serial.printf("Custom image file not found: %s\n", filename.c_str());
    return false;
  }
  
  File file = SPIFFS.open(filename, "r");
  if (!file) {
    Serial.printf("Failed to open file: %s\n", filename.c_str());
    return false;
  }
  
  size_t fileSize = file.size();
  if (fileSize != maxImageSize * 2) { // RGB565は1ピクセル2バイト
    Serial.printf("Invalid file size: %zu (expected: %d)\n", fileSize, maxImageSize * 2);
    file.close();
    return false;
  }
  
  // メモリを確保
  if (customImages[digit] != nullptr) {
    delete[] customImages[digit];
  }
  customImages[digit] = new uint16_t[maxImageSize];
  
  // ファイルから画像データを読み込み
  file.read((uint8_t*)customImages[digit], fileSize);
  file.close();
  
  Serial.printf("Loaded custom image for digit %d\n", digit);
  return true;
}

// 全てのカスタム画像をロードする関数
void loadAllCustomImages() {
  for (int i = 0; i < 10; i++) {
    loadCustomImage(i);
  }
}
void drawNixieDigitOnDisplay(Arduino_GFX* display, int digit) {
  const uint16_t* nixieImages[] = {
    H00, H10, H20, H30, H40, H50, H60, H70, H80, H90
  };
  
  if (digit >= 0 && digit <= 9) {
    // ディスプレイ全体にニキシー管画像を表示
    int16_t screenW = display->width();
    int16_t screenH = display->height();
    
    // カスタム画像を使用するかチェック
    const uint16_t* imageToUse;
    if (useCustomImages && customImages[digit] != nullptr) {
      imageToUse = customImages[digit];
      Serial.printf("Using custom image for digit %d\n", digit);
    } else {
      imageToUse = nixieImages[digit];
      Serial.printf("Using default image for digit %d\n", digit);
    }
    
    // 画像をディスプレイサイズにスケーリング
    scaleImage(imageToUse, iw, ih, imageBuffer, screenW, screenH);
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
    // Serial.println("Display 4 updated: " + String(m1));
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

// 表示リフレッシュ関数
void handleRefresh() {
  // 全てのディスプレイを強制更新
  bh1 = bh0 = bm1 = bm0 = bs1 = bs0 = 99;
  webServer.send(200, "application/json", "{\"success\":true}");
}

// Webサーバーハンドラー関数
void handleRoot() {
  File file = SPIFFS.open("/index.html", "r");
  if (!file) {
    webServer.send(404, "text/plain", "File not found");
    return;
  }
  webServer.streamFile(file, "text/html");
  file.close();
}

void handleUpload() {
  // マルチパート/form-dataを処理
  if (webServer.hasArg("digit")) {
    int digit = webServer.arg("digit").toInt();
    
    if (digit >= 0 && digit <= 9) {
      // HTTPUploadファイルを取得
      HTTPUpload& upload = webServer.upload();
      
      if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Upload Start: %s\n", upload.filename.c_str());
        Serial.printf("Upload for digit: %d\n", digit);
        
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        // ファイルデータを受信中
        Serial.printf("Upload Write: %zu bytes\n", upload.currentSize);
        
      } else if (upload.status == UPLOAD_FILE_END) {
        Serial.printf("Upload End: %zu bytes total\n", upload.totalSize);
        
        // RGB565フォーマットのサイズチェック（170*320*2バイト = 108800バイト）
        const size_t expectedSize = 170 * 320 * 2;
        if (upload.totalSize != expectedSize) {
          Serial.printf("Invalid file size: %zu (expected: %zu)\n", upload.totalSize, expectedSize);
          webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid file size for RGB565 format\"}");
          return;
        }
        
        // 画像データをSPIFFSに保存（RGB565フォーマットとして）
        String filename = "/custom_" + String(digit) + ".rgb565";
        File file = SPIFFS.open(filename, "w");
        if (file) {
          // 注意: この簡易実装では、アップロードされたファイルが
          // 既にRGB565フォーマットであることを前提としています
          // 実際の運用では、JPEG/PNG→RGB565変換が必要です
          file.write(upload.buf, upload.currentSize);
          file.close();
          
          // メモリにもロード
          if (loadCustomImage(digit)) {
            Serial.printf("Saved and loaded custom image for digit %d\n", digit);
            webServer.send(200, "application/json", "{\"success\":true,\"message\":\"Image uploaded successfully\"}");
          } else {
            Serial.printf("Saved but failed to load custom image for digit %d\n", digit);
            webServer.send(500, "application/json", "{\"success\":false,\"message\":\"Failed to load image to memory\"}");
          }
        } else {
          Serial.println("Failed to save file");
          webServer.send(500, "application/json", "{\"success\":false,\"message\":\"Failed to save file\"}");
        }
      }
    } else {
      webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid digit\"}");
    }
  } else {
    webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Missing digit parameter\"}");
  }
}

void handleTime() {
  String timeStr = String(hour) + ":" + String(minute) + ":" + String(second);
  webServer.send(200, "application/json", "{\"time\":\"" + timeStr + "\"}");
}

void handleStatus() {
  // カスタム画像の状態を確認
  String status = "{";
  status += "\"useCustomImages\":" + String(useCustomImages ? "true" : "false") + ",";
  status += "\"customImages\":[";
  for (int i = 0; i < 10; i++) {
    if (i > 0) status += ",";
    status += (customImages[i] != nullptr) ? "true" : "false";
  }
  status += "],";
  status += "\"freeHeap\":" + String(ESP.getFreeHeap());
  status += "}";
  webServer.send(200, "application/json", status);
}

void handleReset() {
  useCustomImages = false;
  // カスタム画像メモリを解放
  for (int i = 0; i < 10; i++) {
    if (customImages[i] != nullptr) {
      delete[] customImages[i];
      customImages[i] = nullptr;
    }
  }
  // 全てのディスプレイを強制更新
  bh1 = bh0 = bm1 = bm0 = bs1 = bs0 = 99;
  webServer.send(200, "application/json", "{\"success\":true}");
}

void handleUseCustom() {
  useCustomImages = true;
  // カスタム画像をロード
  loadAllCustomImages();
  // 全てのディスプレイを強制更新
  bh1 = bh0 = bm1 = bm0 = bs1 = bs0 = 99;
  webServer.send(200, "application/json", "{\"success\":true}");
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
  configTime(9 * 3600, 0, ntpServer); // JST (UTC+9)
}

void setup() {
  Serial.begin(115200);
  Serial.println("Nixie Tube Clock Start (4 Display Test)");
  
  // pinMode(TFT_RST, OUTPUT);
  pinMode(TFT_CS_1, OUTPUT);
  pinMode(TFT_CS_2, OUTPUT);
  pinMode(TFT_CS_3, OUTPUT);
  pinMode(TFT_CS_4, OUTPUT);
  pinMode(TFT_CS_5, OUTPUT);
  pinMode(TFT_CS_6, OUTPUT);
  // digitalWrite(TFT_RST, HIGH); //RESET off
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
  // delay(1000);
  //   for (int i = 0; i < 6; i++) {
  //   displays[i]->begin(10000000);
  //   // displays[i]->fillScreen(BLACK);
  //   // Serial.println("Display " + String(i+1) + " initialized");
  // }
  

  // WiFi接続
  initializeWiFi();
  
  // SPIFFS初期化（画像保存用）
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed!");
  } else {
    Serial.println("SPIFFS initialized successfully");
    // 起動時にカスタム画像をロード
    loadAllCustomImages();
  }
  
  // Webサーバールート設定
  webServer.on("/", handleRoot);
  webServer.on("/upload", HTTP_POST, handleUpload);
  webServer.on("/api/time", handleTime);
  webServer.on("/api/status", handleStatus);
  webServer.on("/api/reset", HTTP_POST, handleReset);
  webServer.on("/api/use-custom", HTTP_POST, handleUseCustom);
  webServer.on("/api/refresh", HTTP_POST, handleRefresh);
  
  // Webサーバー開始
  webServer.begin();
  Serial.println("Web server started");
  Serial.print("Access at: http://");
  Serial.println(WiFi.localIP());
  
  Serial.println("Nixie Tube Clock Ready!");
}

void loop() {
  // Webサーバーリクエスト処理
  webServer.handleClient();
  
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