#include "qr_display.h"

// QRコード表示関連の変数定義
const unsigned long QR_DISPLAY_DURATION = 5000; // 5秒間QRコードを表示
bool qrCodeDisplayed = false;
unsigned long startTime = 0;

// QRコードを全ディスプレイに表示する関数
void displayQRCode(Arduino_GFX* displays[], int displayCount) {
  String ipAddress = WiFi.localIP().toString();
  String fullURL = "http://" + ipAddress;
  Serial.println("Displaying QR code for URL: " + fullURL);
  
  // QRコードを生成
  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(3)]; // version 3 (29x29)
  qrcode_initText(&qrcode, qrcodeData, 3, 0, fullURL.c_str());
  
  // 各ディスプレイにQRコードの一部を表示
  for (int displayIndex = 0; displayIndex < displayCount; displayIndex++) {
    Arduino_GFX* display = displays[displayIndex];
    display->fillScreen(WHITE);
    
    // QRコードサイズ計算
    int moduleSize = 4; // 各モジュールのピクセルサイズ
    int qrSize = qrcode.size * moduleSize;
    
    // 表示開始位置計算（中央に配置）
    int startX = (170 - qrSize) / 2;
    int startY = (320 - qrSize) / 2;
    
    // QRコードを描画（全体を各ディスプレイに表示）
    for (int y = 0; y < qrcode.size; y++) {
      for (int x = 0; x < qrcode.size; x++) {
        bool module = qrcode_getModule(&qrcode, x, y);
        uint16_t color = module ? BLACK : WHITE;
        
        // モジュールサイズ分のピクセルを描画
        for (int py = 0; py < moduleSize; py++) {
          for (int px = 0; px < moduleSize; px++) {
            int pixelX = startX + x * moduleSize + px;
            int pixelY = startY + y * moduleSize + py;
            
            // ディスプレイ範囲内かチェック
            if (pixelX >= 0 && pixelX < 170 && pixelY >= 0 && pixelY < 320) {
              display->drawPixel(pixelX, pixelY, color);
            }
          }
        }
      }
    }
    
    // IPアドレスをテキストで表示（QRコードの下）
    display->setTextColor(BLACK);
    display->setTextSize(1);
    int textY = startY + qrSize + 10;
    if (textY < 310) {
      display->setCursor(10, textY);
      display->print(fullURL);
    }
  }
  
  Serial.println("QR code displayed on all screens");
}

// QRコード表示時間をチェックする関数
bool isQRCodeDisplayTime() {
  return qrCodeDisplayed && (millis() - startTime < QR_DISPLAY_DURATION);
}

// QRコード表示を終了する関数
void endQRCodeDisplay(Arduino_GFX* displays[], int displayCount) {
  if (qrCodeDisplayed) {
    Serial.println("QR code display time finished, switching to clock");
    qrCodeDisplayed = false;
    // 全ディスプレイをクリア
    for (int i = 0; i < displayCount; i++) {
      displays[i]->fillScreen(BLACK);
    }
  }
}
