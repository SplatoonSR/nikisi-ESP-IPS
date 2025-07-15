#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>  // ST7789用ライブラリに変更
#include <SPI.h>

// ESP32-C3の内蔵LED
#define LED_PIN 8

// ST7789 TFTディスプレイピン定義
#define TFT_MOSI  6   // ESP32-C3 SPI MOSI SDA
#define TFT_SCLK  4   // ESP32-C3 SPI SCK  SCL
#define TFT_CS    7   // チップセレクト
#define TFT_RST   3   // リセット
#define TFT_DC    2 // データ/コマンド選択

// ST7789ディスプレイオブジェクト (240x240が一般的)
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

void setup() {
  // シリアル通信を初期化
  Serial.begin(115200);
  
  // LEDピンを出力モードに設定
  pinMode(LED_PIN, OUTPUT);
  
  // ST7789ディスプレイ初期化 (170x320)
  tft.init(170, 320);         // ST7789の初期化
  tft.setRotation(1);         // 画面回転（320x170になる）
  tft.fillScreen(ST77XX_BLACK); // 画面クリア
  
  // 初期表示
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(3);
  tft.println("ESP32-C3");
  tft.println("ST7789 OK!");
  
  Serial.println("ESP32-C3 with ST7789 Start!");
}

void loop() {
  // LEDを点灯
  digitalWrite(LED_PIN, HIGH);
  
  // ディスプレイにLED状態表示（320x170に合わせて調整）
  tft.fillRect(0, 80, 320, 30, ST77XX_BLACK);
  tft.setCursor(0, 80);
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(2);
  tft.println("LED: ON");
  
  Serial.println("LED ON");
  delay(1000);
  
  // LEDを消灯
  digitalWrite(LED_PIN, LOW);
  
  // ディスプレイにLED状態表示
  tft.fillRect(0, 80, 320, 30, ST77XX_BLACK);
  tft.setCursor(0, 80);
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(2);
  tft.println("LED: OFF");
  
  Serial.println("LED OFF");
  delay(1000);
}