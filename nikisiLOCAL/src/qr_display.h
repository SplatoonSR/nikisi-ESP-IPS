#ifndef QR_DISPLAY_H
#define QR_DISPLAY_H

#include <Arduino.h>
#include "Arduino_GFX_Library.h"
#include <WiFi.h>
#include <qrcode.h>

// QRコード表示関連の定数
extern const unsigned long QR_DISPLAY_DURATION; // 5秒間QRコードを表示

// QRコード表示関連の変数
extern bool qrCodeDisplayed;
extern unsigned long startTime;

// 関数プロトタイプ
void displayQRCode(Arduino_GFX* displays[], int displayCount);
bool isQRCodeDisplayTime();
void endQRCodeDisplay(Arduino_GFX* displays[], int displayCount);

#endif // QR_DISPLAY_H
