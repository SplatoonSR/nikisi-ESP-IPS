#include <Arduino.h>
#include "Arduino_GFX_Library.h"
#include "image_data.h"
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <time.h>
#include <DNSServer.h>
#include "config.h"  // WiFi設定をインクルード　*ssid　*password　*server　*timeZoneを外部で定義
#include "qr_display.h"  // QRコード表示関連

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
DNSServer dnsServer;

// WiFi設定関連
bool apMode = false;
const char* apSSID = "NixieClock-Setup";
const char* apPassword = "12345678";
String storedSSID = "";
String storedPassword = "";
const unsigned long WIFI_TIMEOUT = 10000; // 10秒でタイムアウト

// 関数の前方宣言
void startAccessPoint();
void displayAPQRCode();

struct tm localTime;

// 時計変数
int hour = 0, minute = 0, second = 0;
int bh1 = 99, bh0 = 99, bm1 = 99, bm0 = 99, bs1 = 99, bs0 = 99; // 前回の時刻
unsigned long ps_Time = 0;

// 時刻管理の改善
unsigned long lastNtpSync = 0;
const unsigned long NTP_SYNC_INTERVAL = 3600000; // 1時間ごとにNTP同期
bool timeInitialized = false;
unsigned long localTimeOffset = 0; // ローカル時刻のオフセット

// ニキシー管画像サイズ
int iw = 70;   // 幅（デフォルト画像用）
int ih = 134;  // 高さ（デフォルト画像用）

// メモリ効率化：必要最小限のバッファ使用
uint16_t* imageBuffer = nullptr; // 動的に確保する描画バッファ
int16_t scaledWidth, scaledHeight;
int16_t displayX, displayY;

// カスタム画像はSPIFFSから直接読み込み（メモリ節約）
bool useCustomImages = false; // カスタム画像を使用するかのフラグ
int currentImageSet = 0; // 現在使用中の画像セット番号（0-2）
const int MAX_IMAGE_SETS = 3; // 最大画像セット数（軽量化）

// SPIFFS容量管理機能（3セット対応）
void checkSPIFFSCapacity() {
  size_t totalBytes = SPIFFS.totalBytes();
  size_t usedBytes = SPIFFS.usedBytes();
  size_t freeBytes = totalBytes - usedBytes;
  
  Serial.printf("=== SPIFFS Storage Info ===\n");
  Serial.printf("Total: %zu bytes (%.2f KB)\n", totalBytes, totalBytes / 1024.0);
  Serial.printf("Used:  %zu bytes (%.2f KB)\n", usedBytes, usedBytes / 1024.0);
  Serial.printf("Free:  %zu bytes (%.2f KB)\n", freeBytes, freeBytes / 1024.0);
  
  // 1つの画像当たり約18,700バイト（70×134×2バイト）必要
  int imageSize = 70 * 134 * 2;
  int imagesPerSet = 10; // 0-9の10枚
  int bytesPerSet = imageSize * imagesPerSet; // 187,600バイト/セット
  int maxSets = freeBytes / bytesPerSet;
  
  Serial.printf("Image size: %d bytes each\n", imageSize);
  Serial.printf("Images per set: %d\n", imagesPerSet);
  Serial.printf("Bytes per set: %d bytes (%.2f KB)\n", bytesPerSet, bytesPerSet / 1024.0);
  Serial.printf("Can store approximately %d more complete sets (max: %d)\n", maxSets, MAX_IMAGE_SETS);
  
  // 既存セットをカウント
  int existingSets = 0;
  for (int set = 0; set < MAX_IMAGE_SETS; set++) {
    int imagesInSet = 0;
    for (int digit = 0; digit < 10; digit++) {
      String filename = "/set" + String(set) + "_" + String(digit) + ".rgb565";
      if (SPIFFS.exists(filename)) {
        imagesInSet++;
      }
    }
    if (imagesInSet > 0) {
      existingSets++;
      Serial.printf("Set %d: %d images\n", set, imagesInSet);
    }
  }
  Serial.printf("Current active set: %d\n", currentImageSet);
  Serial.printf("Existing sets: %d/%d\n", existingSets, MAX_IMAGE_SETS);
  Serial.println("===========================");
}

// 保存済みカスタム画像の一覧表示（3セット対応）
void listSavedImages() {
  Serial.println("=== Saved Custom Image Sets ===");
  int totalSavedImages = 0;
  
  for (int set = 0; set < MAX_IMAGE_SETS; set++) {
    int imagesInSet = 0;
    Serial.printf("Set %d: ", set);
    
    for (int digit = 0; digit < 10; digit++) {
      String filename = "/set" + String(set) + "_" + String(digit) + ".rgb565";
      if (SPIFFS.exists(filename)) {
        File file = SPIFFS.open(filename, "r");
        if (file) {
          Serial.printf("%d(%zu) ", digit, file.size());
          file.close();
          imagesInSet++;
          totalSavedImages++;
        }
      }
    }
    
    if (imagesInSet > 0) {
      Serial.printf("-> %d/10 images\n", imagesInSet);
    } else {
      Serial.println("-> empty");
    }
  }
  
  Serial.printf("Total saved images: %d\n", totalSavedImages);
  Serial.printf("Current active set: %d\n", currentImageSet);
  Serial.println("===============================");
}

// 設定を保存する関数（セット対応）
void saveSettings() {
  File settingsFile = SPIFFS.open("/settings.json", "w");
  if (settingsFile) {
    JsonDocument doc;
    doc["useCustomImages"] = useCustomImages;
    doc["currentImageSet"] = currentImageSet;
    doc["wifiSSID"] = storedSSID;
    doc["wifiPassword"] = storedPassword;
    
    String settings;
    serializeJson(doc, settings);
    settingsFile.print(settings);
    settingsFile.close();
    Serial.println("Settings saved to SPIFFS (set: " + String(currentImageSet) + ", WiFi: " + storedSSID + ")");
  } else {
    Serial.println("Failed to save settings");
  }
}

// 設定を読み込む関数（セット対応）
void loadSettings() {
  if (SPIFFS.exists("/settings.json")) {
    File settingsFile = SPIFFS.open("/settings.json", "r");
    if (settingsFile) {
      String settings = settingsFile.readString();
      settingsFile.close();
      
      JsonDocument doc;
      deserializeJson(doc, settings);
      
      // useCustomImagesの読み込み
      useCustomImages = doc["useCustomImages"] | false;
      Serial.println("Custom images " + String(useCustomImages ? "enabled" : "disabled") + " from saved settings");
      
      // currentImageSetの読み込み
      int newSet = doc["currentImageSet"] | 0;
      if (newSet >= 0 && newSet < MAX_IMAGE_SETS) {
        currentImageSet = newSet;
        Serial.println("Current image set loaded: " + String(currentImageSet));
      }
      
      // WiFi設定の読み込み
      storedSSID = doc["wifiSSID"] | "";
      storedPassword = doc["wifiPassword"] | "";
      if (storedSSID.length() > 0) {
        Serial.println("Stored WiFi settings loaded: " + storedSSID);
        Serial.println("Stored password length: " + String(storedPassword.length()));
        if (storedPassword.length() > 0) {
          Serial.println("Password starts with: " + storedPassword.substring(0, min(3, (int)storedPassword.length())));
        } else {
          Serial.println("Warning: Stored password is empty!");
        }
      }
    }
  } else {
    Serial.println("No saved settings found, using defaults");
    useCustomImages = false;
    currentImageSet = 0;
    storedSSID = "";
    storedPassword = "";
  }
}
const int maxImageSize = 70 * 134; // 画像の最大サイズ（デフォルトサイズ）
const int maxImageWidth = 70;  // 幅（デフォルトサイズ）
const int maxImageHeight = 134; // 高さ（デフォルトサイズ）

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

// カスタム画像をSPIFFSから直接読み込む関数（メモリ効率版・セット対応）
bool loadCustomImageDirect(int digit, uint16_t* buffer, int bufferSize) {
  String filename = "/set" + String(currentImageSet) + "_" + String(digit) + ".rgb565";
  
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
  const size_t expectedSize = 70 * 134 * 2; // 18,760バイト
  
  // ファイルサイズの許容範囲をチェック（±100バイト）
  if (fileSize < expectedSize - 100 || fileSize > expectedSize + 100) {
    Serial.printf("Invalid file size: %zu (expected around: %zu)\n", fileSize, expectedSize);
    file.close();
    return false;
  }
  
  if (bufferSize < 70 * 134) {
    Serial.printf("Buffer too small: %d (need: %d)\n", bufferSize, 70 * 134);
    file.close();
    return false;
  }
  
  // ファイルから画像データを直接読み込み
  size_t bytesRead = file.read((uint8_t*)buffer, fileSize);
  file.close();
  
  if (bytesRead != fileSize) {
    Serial.printf("Failed to read complete file: read %zu of %zu bytes\n", bytesRead, fileSize);
    return false;
  }
  
  Serial.printf("Successfully loaded custom image for digit %d (%zu bytes)\n", digit, fileSize);
  return true;
}

// カスタム画像ファイルが存在するかチェック
bool hasCustomImage(int digit) {
  String filename = "/set" + String(currentImageSet) + "_" + String(digit) + ".rgb565";
  return SPIFFS.exists(filename);
}
void drawNixieDigitOnDisplay(Arduino_GFX* display, int digit) {
  const uint16_t* nixieImages[] = {
    H00, H10, H20, H30, H40, H50, H60, H70, H80, H90
  };
  
  if (digit >= 0 && digit <= 9) {
    // ディスプレイ全体にニキシー管画像を表示
    int16_t screenW = display->width();
    int16_t screenH = display->height();
    
    // 描画バッファを動的に確保
    int bufferSize = screenW * screenH;
    if (imageBuffer == nullptr) {
      imageBuffer = new(std::nothrow) uint16_t[bufferSize];
      if (imageBuffer == nullptr) {
        Serial.printf("Failed to allocate image buffer (%d bytes)\n", bufferSize * 2);
        return;
      }
      Serial.printf("Allocated image buffer: %d bytes\n", bufferSize * 2);
    }
    
    // カスタム画像を使用するかチェック
    bool useCustom = useCustomImages && hasCustomImage(digit);
    
    if (useCustom) {
      // カスタム画像をSPIFFSから直接読み込み（70×134サイズ）
      uint16_t* tempBuffer = new(std::nothrow) uint16_t[70 * 134];
      if (tempBuffer != nullptr) {
        if (loadCustomImageDirect(digit, tempBuffer, 70 * 134)) {
          // カスタム画像をディスプレイサイズにスケーリング
          scaleImage(tempBuffer, maxImageWidth, maxImageHeight, imageBuffer, screenW, screenH);
          Serial.printf("Using custom image for digit %d (70x134)\n", digit);
        } else {
          // カスタム画像読み込み失敗時はデフォルト画像を使用
          scaleImage(nixieImages[digit], iw, ih, imageBuffer, screenW, screenH);
          Serial.printf("Failed to load custom image, using default for digit %d\n", digit);
        }
        delete[] tempBuffer;
      } else {
        // メモリ確保失敗時はデフォルト画像を使用
        scaleImage(nixieImages[digit], iw, ih, imageBuffer, screenW, screenH);
        Serial.printf("Memory allocation failed, using default for digit %d\n", digit);
      }
    } else {
      // デフォルト画像を使用
      scaleImage(nixieImages[digit], iw, ih, imageBuffer, screenW, screenH);
      Serial.printf("Using default image for digit %d (70x134)\n", digit);
    }
    
    // 画像を描画
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

// 新しいAPI: セット切り替え
void handleSetSwitch() {
  if (webServer.hasArg("set")) {
    int newSet = webServer.arg("set").toInt();
    if (newSet >= 0 && newSet < MAX_IMAGE_SETS) {
      currentImageSet = newSet;
      saveSettings();
      
      // 全てのディスプレイを強制更新
      bh1 = bh0 = bm1 = bm0 = bs1 = bs0 = 99;
      
      webServer.send(200, "application/json", "{\"success\":true,\"currentSet\":" + String(currentImageSet) + "}");
      Serial.println("Switched to image set: " + String(currentImageSet));
    } else {
      webServer.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid set number\"}");
    }
  } else {
    webServer.send(400, "application/json", "{\"success\":false,\"error\":\"Missing set parameter\"}");
  }
}

// 新しいAPI: 利用可能なセット一覧を取得
void handleGetSets() {
  String response = "{\"currentSet\":" + String(currentImageSet) + ",\"sets\":[";
  
  for (int set = 0; set < MAX_IMAGE_SETS; set++) {
    if (set > 0) response += ",";
    
    // セット内の画像数をカウント
    int imageCount = 0;
    for (int digit = 0; digit < 10; digit++) {
      String filename = "/set" + String(set) + "_" + String(digit) + ".rgb565";
      if (SPIFFS.exists(filename)) {
        imageCount++;
      }
    }
    
    response += "{\"id\":" + String(set) + ",\"imageCount\":" + String(imageCount) + "}";
  }
  
  response += "]}";
  webServer.send(200, "application/json", response);
}

// 新しいAPI: 指定セットの削除
void handleDeleteSet() {
  if (webServer.hasArg("set")) {
    int targetSet = webServer.arg("set").toInt();
    if (targetSet >= 0 && targetSet < MAX_IMAGE_SETS) {
      int deletedCount = 0;
      
      // 指定セットの全画像を削除
      for (int digit = 0; digit < 10; digit++) {
        String filename = "/set" + String(targetSet) + "_" + String(digit) + ".rgb565";
        if (SPIFFS.exists(filename)) {
          if (SPIFFS.remove(filename)) {
            deletedCount++;
            Serial.printf("Deleted: %s\n", filename.c_str());
          }
        }
      }
      
      // 削除したセットが現在のセットの場合、セット0に切り替え
      if (targetSet == currentImageSet) {
        currentImageSet = 0;
        useCustomImages = false; // デフォルト画像に戻す
        saveSettings();
        // 全てのディスプレイを強制更新
        bh1 = bh0 = bm1 = bm0 = bs1 = bs0 = 99;
      }
      
      String message = "Deleted " + String(deletedCount) + " images from set " + String(targetSet);
      webServer.send(200, "application/json", "{\"success\":true,\"message\":\"" + message + "\"}");
      Serial.println("Set " + String(targetSet) + " deleted: " + String(deletedCount) + " files");
    } else {
      webServer.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid set number\"}");
    }
  } else {
    webServer.send(400, "application/json", "{\"success\":false,\"error\":\"Missing set parameter\"}");
  }
}

// Webサーバーハンドラー関数
void handleRoot() {
  if (apMode) {
    // APモード用の設定画面を表示
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>WiFi Setup</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:Arial;margin:40px;background:#f0f0f0}";
    html += ".container{background:white;padding:20px;border-radius:10px;box-shadow:0 4px 6px rgba(0,0,0,0.1)}";
    html += "input{width:100%;padding:10px;margin:10px 0;border:1px solid #ddd;border-radius:5px}";
    html += "button{background:#007cba;color:white;padding:12px 20px;border:none;border-radius:5px;cursor:pointer;width:100%}";
    html += "button:hover{background:#005a8b}</style></head><body>";
    html += "<div class='container'><h2>🕐 ニキシー管時計 WiFi設定</h2>";
    html += "<p>新しいWiFiネットワークに接続するための設定を入力してください。</p>";
    html += "<form action='/wifi-config' method='POST'>";
    html += "<label>WiFi SSID:</label><input type='text' name='ssid' required>";
    html += "<label>パスワード:</label><input type='password' name='password' required>";
    html += "<button type='submit'>接続</button></form>";
    html += "<hr><p><small>現在のアクセスポイント: " + String(apSSID) + "</small></p></div></body></html>";
    webServer.send(200, "text/html", html);
  } else {
    // 通常モード用のメイン画面
    File file = SPIFFS.open("/index.html", "r");
    if (!file) {
      webServer.send(404, "text/plain", "File not found");
      return;
    }
    webServer.streamFile(file, "text/html");
    file.close();
  }
}

// WiFi設定を処理
void handleWiFiConfig() {
  if (webServer.hasArg("ssid") && webServer.hasArg("password")) {
    String newSSID = webServer.arg("ssid");
    String newPassword = webServer.arg("password");
    
    Serial.println("New WiFi config received: " + newSSID);
    
    // 設定を保存
    storedSSID = newSSID;
    storedPassword = newPassword;
    saveSettings();
    
    // 成功画面を表示
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>WiFi Setup</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:Arial;margin:40px;background:#f0f0f0;text-align:center}";
    html += ".container{background:white;padding:20px;border-radius:10px;box-shadow:0 4px 6px rgba(0,0,0,0.1)}";
    html += ".success{color:#28a745;font-size:18px}</style></head><body>";
    html += "<div class='container'><h2>✅ 設定完了</h2>";
    html += "<p class='success'>WiFi設定が保存されました！</p>";
    html += "<p>デバイスが新しいネットワークに接続します。<br>接続完了後、新しいIPアドレスでアクセスしてください。</p>";
    html += "<p><small>約30秒後に自動的に再起動します...</small></p></div></body></html>";
    webServer.send(200, "text/html", html);
    
    // 少し待ってから再起動
    delay(3000);
    ESP.restart();
  } else {
    webServer.send(400, "text/plain", "Missing parameters");
  }
}

// WiFi情報を取得
void handleWiFiInfo() {
  String response = "{";
  response += "\"mode\":\"" + String(apMode ? "AP" : "STA") + "\",";
  if (apMode) {
    response += "\"apSSID\":\"" + String(apSSID) + "\",";
    response += "\"apIP\":\"" + WiFi.softAPIP().toString() + "\",";
    response += "\"clients\":" + String(WiFi.softAPgetStationNum());
  } else {
    response += "\"ssid\":\"" + WiFi.SSID() + "\",";
    response += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    response += "\"rssi\":" + String(WiFi.RSSI());
  }
  response += "}";
  webServer.send(200, "application/json", response);
}

void handleUpload() {
  HTTPUpload& upload = webServer.upload();
  static File uploadFile;
  static int currentDigit = -1;
  
  if (upload.status == UPLOAD_FILE_START) {
    // digitパラメータを取得
    if (webServer.hasArg("digit")) {
      currentDigit = webServer.arg("digit").toInt();
      
      if (currentDigit >= 0 && currentDigit <= 9) {
        Serial.printf("Upload Start: %s for digit %d\n", upload.filename.c_str(), currentDigit);
        
        String filename = "/set" + String(currentImageSet) + "_" + String(currentDigit) + ".rgb565";
        uploadFile = SPIFFS.open(filename, "w");
        
        if (!uploadFile) {
          Serial.println("Failed to open file for writing");
          return; // エラーはUPLOAD_FILE_ENDで処理
        }
      } else {
        Serial.printf("Invalid digit: %d\n", currentDigit);
        currentDigit = -1;
        return;
      }
    } else {
      Serial.println("Missing digit parameter");
      currentDigit = -1;
      return;
    }
    
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile && currentDigit >= 0) {
      size_t written = uploadFile.write(upload.buf, upload.currentSize);
      if (written != upload.currentSize) {
        Serial.printf("Write error: wrote %zu of %zu bytes\n", written, upload.currentSize);
      }
    }
    
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
    }
    
    Serial.printf("Upload End: %zu bytes total\n", upload.totalSize);
    
    if (currentDigit < 0 || currentDigit > 9) {
      webServer.send(400, "application/json", 
                    "{\"success\":false,\"message\":\"Invalid digit parameter\"}");
      return;
    }
    
    // 期待されるファイルサイズをチェック
    const size_t expectedSize = 70 * 134 * 2; // 18,760バイト
    
    // RGB565ファイルの場合は正確なサイズを要求
    if (upload.filename.endsWith(".rgb565")) {
      if (upload.totalSize != expectedSize) {
        Serial.printf("Invalid RGB565 file size: %zu (expected: %zu)\n", upload.totalSize, expectedSize);
        webServer.send(400, "application/json", 
                      "{\"success\":false,\"message\":\"Invalid file size. Expected 70x134 RGB565 format (18760 bytes)\"}");
        return;
      }
    } else {
      // 変換された画像の場合は少し範囲を広げる（±100バイト）
      if (upload.totalSize < expectedSize - 100 || upload.totalSize > expectedSize + 100) {
        Serial.printf("Invalid converted image size: %zu (expected around: %zu)\n", upload.totalSize, expectedSize);
        webServer.send(400, "application/json", 
                      "{\"success\":false,\"message\":\"Invalid converted image size. Expected around 70x134 RGB565 format\"}");
        return;
      }
    }
    
    // メモリへのロードは不要（ファイル保存のみ）
    Serial.printf("Successfully saved custom image for digit %d\n", currentDigit);
    webServer.send(200, "application/json", 
                  "{\"success\":true,\"message\":\"Image uploaded and saved successfully\"}");
    
    // リセット
    currentDigit = -1;
  }
}

void handleTime() {
  // 時刻文字列を統一フォーマットで生成
  char timeStr[16];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", hour, minute, second);
  
  String response = "{\"time\":\"" + String(timeStr) + "\",\"initialized\":" + 
                   String(timeInitialized ? "true" : "false") + "}";
  webServer.send(200, "application/json", response);
}

void handleStatus() {
  // カスタム画像の状態を確認（3セット対応）
  String status = "{";
  status += "\"useCustomImages\":" + String(useCustomImages ? "true" : "false") + ",";
  status += "\"currentImageSet\":" + String(currentImageSet) + ",";
  status += "\"customImages\":[";
  for (int i = 0; i < 10; i++) {
    if (i > 0) status += ",";
    status += hasCustomImage(i) ? "true" : "false";
  }
  status += "],";
  status += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
  
  // SPIFFS容量情報を追加（3セット対応）
  size_t totalBytes = SPIFFS.totalBytes();
  size_t usedBytes = SPIFFS.usedBytes();
  size_t freeBytes = totalBytes - usedBytes;
  int imageSize = 70 * 134 * 2; // 1画像のサイズ
  int maxImagesPerSet = 10; // セットあたりの最大画像数
  int maxSets = MAX_IMAGE_SETS; // 最大セット数
  int currentUsedSets = 0;
  
  // 使用中のセット数をカウント
  for (int set = 0; set < MAX_IMAGE_SETS; set++) {
    for (int digit = 0; digit < 10; digit++) {
      String filename = "/set" + String(set) + "_" + String(digit) + ".rgb565";
      if (SPIFFS.exists(filename)) {
        currentUsedSets = set + 1;
        break;
      }
    }
  }
  
  status += "\"spiffs\":{";
  status += "\"total\":" + String(totalBytes) + ",";
  status += "\"used\":" + String(usedBytes) + ",";
  status += "\"free\":" + String(freeBytes) + ",";
  status += "\"imageSize\":" + String(imageSize) + ",";
  status += "\"maxSets\":" + String(maxSets) + ",";
  status += "\"usedSets\":" + String(currentUsedSets) + ",";
  status += "\"imagesPerSet\":" + String(maxImagesPerSet);
  status += "}";
  
  status += "}";
  webServer.send(200, "application/json", status);
}

void handleReset() {
  useCustomImages = false;
  saveSettings(); // 設定を保存
  
  // カスタム画像ファイルは削除せず、使用フラグのみリセット
  // これにより画像は保持されるが、デフォルト画像が表示される
  
  // 全てのディスプレイを強制更新
  bh1 = bh0 = bm1 = bm0 = bs1 = bs0 = 99;
  webServer.send(200, "application/json", "{\"success\":true,\"message\":\"Switched to default images (custom images preserved)\"}");
  Serial.println("Switched to default images, custom images preserved");
}

// 新しいAPI: カスタム画像を完全削除（3セット対応）
void handleDeleteAll() {
  useCustomImages = false;
  currentImageSet = 0;
  saveSettings(); // 設定を保存
  
  // 全てのセットのカスタム画像ファイルを削除
  int deletedCount = 0;
  for (int set = 0; set < MAX_IMAGE_SETS; set++) {
    for (int i = 0; i < 10; i++) {
      String filename = "/set" + String(set) + "_" + String(i) + ".rgb565";
      if (SPIFFS.exists(filename)) {
        if (SPIFFS.remove(filename)) {
          deletedCount++;
          Serial.printf("Deleted custom image file: %s\n", filename.c_str());
        }
      }
    }
  }
  
  // 全てのディスプレイを強制更新
  bh1 = bh0 = bm1 = bm0 = bs1 = bs0 = 99;
  
  String message = "Deleted " + String(deletedCount) + " custom image(s) from all sets";
  webServer.send(200, "application/json", "{\"success\":true,\"message\":\"" + message + "\"}");
  Serial.println("All custom images deleted: " + String(deletedCount) + " files");
}

void handleUseCustom() {
  useCustomImages = true;
  saveSettings(); // 設定を保存
  
  // 全てのディスプレイを強制更新（カスタム画像は必要時に読み込み）
  bh1 = bh0 = bm1 = bm0 = bs1 = bs0 = 99;
  webServer.send(200, "application/json", "{\"success\":true}");
}

// WiFi接続関数
void initializeWiFi() {
  bool connected = false;
  
  // WiFiネットワークをスキャンして利用可能なネットワークを表示
  Serial.println("Scanning for available WiFi networks...");
  int networkCount = WiFi.scanNetworks();
  Serial.println("Found " + String(networkCount) + " networks:");
  for (int i = 0; i < networkCount; i++) {
    Serial.printf("  %d: %s (RSSI: %d) %s\n", i, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "Open" : "Encrypted");
  }
  Serial.println("---");
  
  // 1. 保存されたWiFi設定を最初に試行
  if (storedSSID.length() > 0 && storedPassword.length() > 0) {
    Serial.println("Trying saved WiFi settings: " + storedSSID);
    Serial.println("Saved password length: " + String(storedPassword.length()));
    Serial.println("Saved password (first 3 chars): " + storedPassword.substring(0, min(3, (int)storedPassword.length())) + "...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(storedSSID.c_str(), storedPassword.c_str());
    
    unsigned long startTime = millis();
    Serial.print("WiFi connecting to saved network");
    
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_TIMEOUT) {
      delay(500);
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      Serial.println("");
      Serial.println("WiFi connected to saved network!");
    } else {
      Serial.println("");
      Serial.println("Saved WiFi connection failed. Status: " + String(WiFi.status()));
      Serial.println("Saved WiFi settings failed, trying config.h settings...");
    }
  } else {
    Serial.println("No saved WiFi settings found (SSID: '" + storedSSID + "', Password length: " + String(storedPassword.length()) + ")");
    Serial.println("Skipping to config.h settings...");
  }
  
  // 2. 保存された設定で失敗した場合、config.hの設定を試行（無効化）
  if (!connected && false) { // config.hでの接続を無効化
    Serial.println("Trying config.h WiFi settings: " + String(ssid));
    Serial.println("Config.h password length: " + String(strlen(password)));
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    unsigned long startTime = millis();
    Serial.print("WiFi connecting to config.h network");
    
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_TIMEOUT) {
      delay(500);
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      Serial.println("");
      Serial.println("WiFi connected to config.h network!");
    } else {
      Serial.println("");
      Serial.println("Config.h WiFi connection failed. Status: " + String(WiFi.status()));
      Serial.println("Config.h WiFi settings also failed.");
    }
  }
  
  if (connected) {
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    apMode = false;
    
    // NTP時刻同期を改善
    Serial.println("Synchronizing time with NTP server...");
    configTime(9 * 3600, 0, ntpServer); // JST (UTC+9)
    
    // NTP同期を待機
    struct tm timeinfo;
    int retryCount = 0;
    while (!getLocalTime(&timeinfo) && retryCount < 20) {
      Serial.print(".");
      delay(1000);
      retryCount++;
    }
    
    if (retryCount < 20) {
      Serial.println("\nNTP time synchronized successfully!");
      hour = timeinfo.tm_hour;
      minute = timeinfo.tm_min;
      second = timeinfo.tm_sec;
      timeInitialized = true;
      lastNtpSync = millis();
      localTimeOffset = millis() - (hour * 3600 + minute * 60 + second) * 1000;
      Serial.printf("Initial time: %02d:%02d:%02d\n", hour, minute, second);
    } else {
      Serial.println("\nNTP sync failed, using manual time");
      // デフォルト時刻を設定
      hour = 12;
      minute = 0;
      second = 0;
      timeInitialized = false;
    }
  } else {
    Serial.println("All WiFi connection attempts failed. Starting Access Point mode...");
    startAccessPoint();
  }
}

// アクセスポイントモードを開始
void startAccessPoint() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, apPassword);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  apMode = true;
  
  // DNSサーバーを開始（キャプティブポータル用）
  dnsServer.start(53, "*", IP);
  
  Serial.println("Access Point started!");
  Serial.println("Connect to WiFi: " + String(apSSID));
  Serial.println("Password: " + String(apPassword));
  Serial.println("Then open browser and go to: http://" + IP.toString());
  
  // APモード用のQRコードを表示
  displayAPQRCode();
}

// APモード用のQRコード表示
void displayAPQRCode() {
  // APモードの接続情報をQRコードで表示
  String apInfo = "WIFI:T:WPA;S:" + String(apSSID) + ";P:" + String(apPassword) + ";;";
  Serial.println("Displaying AP QR code for: " + apInfo);
  
  for (int displayIndex = 0; displayIndex < 6; displayIndex++) {
    Arduino_GFX* display = displays[displayIndex];
    display->fillScreen(WHITE);
    
    // APモード表示用の簡単なテキスト
    display->setTextColor(BLACK);
    display->setTextSize(1);
    display->setCursor(5, 50);
    display->println("WiFi Setup");
    display->setCursor(5, 70);
    display->println("SSID:");
    display->setCursor(5, 90);
    display->println(apSSID);
    display->setCursor(5, 110);
    display->println("PASS:");
    display->setCursor(5, 130);
    display->println(apPassword);
    display->setCursor(5, 160);
    display->println("Setup:");
    display->setCursor(5, 180);
    display->println(WiFi.softAPIP().toString());
  }
  
  Serial.println("AP setup info displayed on all screens");
}

// 安定した時刻取得関数
void updateTimeStable() {
  if (timeInitialized) {
    // NTP同期済みの場合、ローカルカウンターを使用
    unsigned long currentMillis = millis();
    unsigned long elapsedSeconds = (currentMillis - localTimeOffset) / 1000;
    
    second = elapsedSeconds % 60;
    minute = (elapsedSeconds / 60) % 60;
    hour = (elapsedSeconds / 3600) % 24;
    
    // 定期的なNTP再同期
    if (currentMillis - lastNtpSync > NTP_SYNC_INTERVAL) {
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        // 大きなずれがある場合のみ補正
        int ntpHour = timeinfo.tm_hour;
        int ntpMinute = timeinfo.tm_min;
        int ntpSecond = timeinfo.tm_sec;
        
        int timeDiff = abs((ntpHour * 3600 + ntpMinute * 60 + ntpSecond) - 
                          (hour * 3600 + minute * 60 + second));
        
        if (timeDiff > 2) { // 2秒以上のずれがある場合
          hour = ntpHour;
          minute = ntpMinute;
          second = ntpSecond;
          localTimeOffset = currentMillis - (hour * 3600 + minute * 60 + second) * 1000;
          Serial.printf("Time corrected: %02d:%02d:%02d (diff: %d sec)\n", hour, minute, second, timeDiff);
        }
        lastNtpSync = currentMillis;
      }
    }
  } else {
    // NTP未同期の場合、手動カウンター
    static unsigned long lastSecondUpdate = 0;
    if (millis() - lastSecondUpdate >= 1000) {
      second++;
      if (second >= 60) {
        second = 0;
        minute++;
        if (minute >= 60) {
          minute = 0;
          hour++;
          if (hour >= 24) {
            hour = 0;
          }
        }
      }
      lastSecondUpdate = millis();
      
      // 定期的にNTP同期を試行
      if (millis() - lastNtpSync > 60000) { // 1分ごと
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
          hour = timeinfo.tm_hour;
          minute = timeinfo.tm_min;
          second = timeinfo.tm_sec;
          timeInitialized = true;
          localTimeOffset = millis() - (hour * 3600 + minute * 60 + second) * 1000;
          Serial.println("NTP sync recovered!");
        }
        lastNtpSync = millis();
      }
    }
  }
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
  

  // SPIFFS初期化（画像保存用）
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed!");
  } else {
    Serial.println("SPIFFS initialized successfully");
    // メモリ節約のため起動時のロードは行わない
    Serial.printf("Free heap after SPIFFS init: %d bytes\n", ESP.getFreeHeap());
    
    // 設定を読み込み（WiFi初期化前に実行）
    loadSettings();
    
    // 保存済み画像とストレージ情報を表示
    listSavedImages();
    checkSPIFFSCapacity();
  }
  
  // WiFi接続（設定読み込み後に実行）
  initializeWiFi();
  
  // Webサーバールート設定
  webServer.on("/", handleRoot);
  webServer.on("/wifi-config", HTTP_POST, handleWiFiConfig);  // WiFi設定
  webServer.on("/api/wifi-info", handleWiFiInfo);  // WiFi情報取得
  webServer.on("/upload", HTTP_POST, 
    []() { 
      // POSTレスポンスはhandleUpload内で処理される
    }, 
    handleUpload); // アップロードハンドラーを正しく設定
  webServer.on("/api/time", handleTime);
  webServer.on("/api/status", handleStatus);
  webServer.on("/api/reset", HTTP_POST, handleReset);  // デフォルト画像に切り替え（画像保持）
  webServer.on("/api/delete-all", HTTP_POST, handleDeleteAll);  // 全カスタム画像削除
  webServer.on("/api/use-custom", HTTP_POST, handleUseCustom);
  webServer.on("/api/refresh", HTTP_POST, handleRefresh);
  
  // 3セット対応の新しいAPIエンドポイント
  webServer.on("/api/set-switch", HTTP_POST, handleSetSwitch);  // セット切り替え
  webServer.on("/api/get-sets", handleGetSets);  // セット一覧取得
  webServer.on("/api/delete-set", HTTP_POST, handleDeleteSet);  // 指定セット削除
  
  // キャプティブポータル用（APモード時）
  webServer.onNotFound([]() {
    if (apMode) {
      handleRoot();  // APモード時は全てのリクエストを設定画面にリダイレクト
    } else {
      webServer.send(404, "text/plain", "Not found");
    }
  });
  
  // Webサーバー開始
  webServer.begin();
  Serial.println("Web server started");
  
  if (!apMode) {
    Serial.print("Access at: http://");
    Serial.println(WiFi.localIP());
    
    // WiFi接続成功時のみQRコードを5秒間表示
    displayQRCode(displays, 6);
    startTime = millis();
    qrCodeDisplayed = true;
  } else {
    Serial.print("Setup at: http://");
    Serial.println(WiFi.softAPIP());
    // APモード時はQRコード表示はしない（既に設定画面が表示されているため）
    qrCodeDisplayed = false;
  }
  
  Serial.println("Nixie Tube Clock Ready!");
}

void loop() {
  // APモード時はDNSサーバーも処理
  if (apMode) {
    dnsServer.processNextRequest();
  }
  
  // Webサーバーリクエスト処理
  webServer.handleClient();
  
  // WiFi接続チェック（STAモード時のみ）
  if (!apMode && WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    initializeWiFi();
  }
  
  // QRコード表示時間チェック
  if (qrCodeDisplayed && (millis() - startTime >= QR_DISPLAY_DURATION)) {
    endQRCodeDisplay(displays, 6);
    // 前回の時刻をリセットして強制更新
    bh1 = 99; bh0 = 99; bm1 = 99; bm0 = 99; bs1 = 99; bs0 = 99;
  }
  
  // QRコード表示中は時計表示をスキップ
  // APモード時は時計表示もスキップ
  if (!qrCodeDisplayed && !apMode) {
    // より安定した時刻更新（100ms間隔でチェック、1秒ごとに表示更新）
    static unsigned long lastTimeUpdate = 0;
    if (millis() - lastTimeUpdate >= 100) { // 100ms間隔で時刻チェック
      int prevSecond = second;
      updateTimeStable(); // 安定した時刻取得
      
      // 秒が変わった場合のみ表示を更新
      if (second != prevSecond) {
        Serial.printf("%02d:%02d:%02d\n", hour, minute, second);
        updateClock();
      }
      
      lastTimeUpdate = millis();
    }
  }
  
  delay(50); // CPU負荷軽減
}