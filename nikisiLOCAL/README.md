# Nixie Tube Style Digital Clock

ESP32-C3 と 6 個の ST7789 TFT ディスプレイを使用したニキシー管風デジタル時計プロジェクト

## 📋 プロジェクト概要

このプロジェクトは、ESP32-C3 マイコンと 6 個の ST7789 TFT ディスプレイを使用して、ニキシー管風のデジタル時計を作成します。WiFi 経由で NTP サーバーから正確な時刻を取得し、HH:MM:SS 形式で表示します。

## ✨ 特徴

- **6 ディスプレイ表示**: 時:分:秒の 6 桁をそれぞれ独立したディスプレイで表示
- **ニキシー管風デザイン**: レトロな雰囲気のニキシー管画像を使用
- **WiFi 時刻同期**: NTP サーバーから自動で正確な時刻を取得
- **省電力設計**: 変更があった桁のみ更新する効率的な表示更新
- **セキュア設定**: WiFi 認証情報は外部ファイルで管理

## 🛠️ ハードウェア構成

### 必要な部品

- ESP32-C3 DevKit-M-1 × 1
- ST7789 TFT ディスプレイ (170×320) × 6
- 10kΩ 抵抗 × 1 (RST pin 用プルアップ)
- ジャンパーワイヤー
- ブレッドボード

### ディスプレイ配置

```
[ディスプレイ6] [ディスプレイ5] [ディスプレイ4] [ディスプレイ3] [ディスプレイ2] [ディスプレイ1]
    時十の位      時一の位      分十の位      分一の位      秒十の位      秒一の位
    (GPIO21)     (GPIO20)     (GPIO10)      (GPIO9)       (GPIO8)       (GPIO7)
```

## 🔌 配線図

### ESP32-C3 ピン配置

| 機能 | ESP32-C3 Pin | 接続先                                 |
| ---- | ------------ | -------------------------------------- |
| MOSI | GPIO6        | 全ディスプレイの SDA                   |
| SCLK | GPIO4        | 全ディスプレイの SCL                   |
| DC   | GPIO2        | 全ディスプレイの DC                    |
| RST  | -            | 全ディスプレイの RST + 10kΩ プルアップ |
| CS1  | GPIO7        | ディスプレイ 1 の CS (秒一の位)        |
| CS2  | GPIO8        | ディスプレイ 2 の CS (秒十の位)        |
| CS3  | GPIO9        | ディスプレイ 3 の CS (分一の位)        |
| CS4  | GPIO10       | ディスプレイ 4 の CS (分十の位)        |
| CS5  | GPIO20       | ディスプレイ 5 の CS (時一の位)        |
| CS6  | GPIO21       | ディスプレイ 6 の CS (時十の位)        |

### 重要な注意事項

⚠️ **RST pin は 10kΩ で VCC にプルアップしてください**  
⚠️ **GPIO20, 21 は ESP32-C3 で不安定な場合があります（GPIO5, 3 への変更を推奨）**

## 💻 ソフトウェア設定

### 開発環境

- PlatformIO
- Arduino Framework
- Arduino_GFX Library v1.6.0

### 必要なライブラリ

```ini
[lib_deps]
moononournation/GFX Library for Arduino@^1.6.0
```

### WiFi 設定

1. `src/config.h.example` を `src/config.h` にコピー
2. WiFi 認証情報を設定:

```cpp
// WiFi設定
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// NTP設定
const char* server = "ntp.nict.jp";
const int timeZone = 9; // JST (UTC+9)
```

## 🚀 ビルド・実行方法

### 1. 環境準備

```bash
# PlatformIOプロジェクトをクローン
git clone <repository_url>
cd nikisiLOCAL

# config.hを設定
cp src/config.h.example src/config.h
# エディタでconfig.hを編集してWiFi情報を入力
```

### 2. ビルド・アップロード

```bash
# ビルド
pio run

# ESP32-C3にアップロード
pio run -t upload

# シリアルモニター
pio device monitor
```

## 📁 ファイル構成

```
nikisiLOCAL/
├── src/
│   ├── main.cpp              # メインプログラム
│   ├── config.h              # WiFi設定 (gitignore対象)
│   ├── config.h.example      # WiFi設定テンプレート
│   └── image_data.h          # ニキシー管画像データ
├── include/
├── lib/
├── test/
├── platformio.ini            # PlatformIO設定
└── README.md                 # このファイル
```

## 🔧 トラブルシューティング

### よくある問題と解決方法

#### 1. ディスプレイにノイズが表示される

- **原因**: 電源供給不足、信号品質劣化
- **解決**:
  - 各ディスプレイに 100nF バイパスコンデンサを追加
  - ESP32 の電源に 1000μF コンデンサを追加
  - GPIO20, 21 を GPIO5, 3 に変更

#### 2. 一部のディスプレイが動作しない

- **原因**: CS pin の競合、配線ミス
- **解決**:
  - 配線を再確認
  - 各ディスプレイの CS pin が正しく接続されているか確認

#### 3. WiFi 接続エラー

- **原因**: SSID/パスワードの間違い、WiFi 規格非対応
- **解決**:
  - config.h の設定を再確認
  - 2.4GHz WiFi を使用しているか確認

#### 4. 時刻が表示されない

- **原因**: NTP サーバー接続失敗
- **解決**:
  - インターネット接続を確認
  - NTP サーバーアドレスを変更 (`pool.ntp.org`など)

## 📈 カスタマイズ

### 表示順序の変更

`updateClock()` 関数内のディスプレイ割り当てを変更することで、表示順序をカスタマイズできます。

### ニキシー管画像の変更

`image_data.h` ファイル内の画像データを変更することで、異なるデザインのニキシー管を使用できます。

### タイムゾーンの変更

`config.h` の `timeZone` 値を変更することで、任意のタイムゾーンに対応できます。

## 📝 ライセンス

このプロジェクトは MIT ライセンスの下で公開されています。

## 🤝 コントリビューション

プルリクエストやイシューの報告を歓迎します。改善提案がございましたら、お気軽にお知らせください。

## 📞 サポート

技術的な質問や問題については、GitHub の Issues ページでお尋ねください。

---

**最終更新**: 2025 年 7 月 23 日
