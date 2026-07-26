# STICKCPLUS2-THI-AirGate  
GitHub Copilot / 開発引き継ぎ文書

---

# プロジェクト概要

M5Stack StickCPlus2 を利用し、  
「リビングルーム」と「エントランス（玄関）」の THI 値を比較し、  
室内ドアの OPEN/CLOSE 判断を表示する IoT 端末。

目的は：

- 外気（玄関側）が快適なら空気を流入
- 外気（玄関側）が不快なら遮断

を、人間が瞬時に判断できる UI として構築すること。

これは単なる温湿度表示端末ではなく、

```text
空気移動の意思決定UI
```

として設計されている。

---

# ハードウェア

使用デバイス：

```text
M5Stack StickCPlus2
```

特徴：

- 小型 TFT
- Wi-Fi
- MQTT subscriber 用途
- ドア横設置を想定

---

# MQTT Topics

## リビング側

```text
home/co2/aggregate/raw
```

サンプル：

```json
{
  "thi": 72.7
}
```

実際には多数のフィールドを含むが、
必要なのは `thi` のみ。

---

## エントランス側

```text
home/env/env4-entrance/raw
```

サンプル：

```json
{
  "thi": 71.6
}
```

---

# 判定ロジック

## 基本思想

```text
livingTHI - entranceTHI
```

を比較する。

---

## OPEN条件

玄関側の THI が十分低い場合：

```text
livingTHI - entranceTHI >= 0.5
```

→ `"OPEN"`

意味：

```text
玄関の空気をリビングへ流入させる価値あり
```

---

## CLOSE条件

差が小さい、
または玄関側が不快な場合：

```text
livingTHI - entranceTHI <= 0.2
```

→ `"CLOSE"`

意味：

```text
玄関の熱気・湿気を遮断
```

---

# ヒステリシス

重要。

単純比較だと、

```text
OPEN
CLOSE
OPEN
CLOSE
```

が高速反転する。

そのため：

| 状態 | 条件 |
|---|---|
| OPENへ遷移 | diff >= 0.5 |
| CLOSEへ遷移 | diff <= 0.2 |

としている。

---

# Display設計

StickCPlus2 は画面が小さい。

そのため：

```text
情報量を増やしすぎない
```

ことを重視。

---

# 現在の表示仕様

## OPEN時

```text
OPEN
dTHI: 1.1
L:72.7 E:71.6
```

背景：

- 黒

文字色：

- 緑

---

## CLOSE時

```text
CLOSE
dTHI: -0.8
L:71.2 E:72.0
```

文字色：

- 赤

---

# 採用しない設計

意図的に除外：

- CO2値表示
- RSSI表示
- uptime表示
- node数表示
- debugログ大量表示

理由：

```text
判断端末をデバッグ端末化しない
```

ため。

---

# ソフトウェア構成

## ファイル構成

```text
STICKCPLUS2-THI-AirGate/
├── STICKCPLUS2-THI-AirGate.ino
└── config.h
```

---

# config.h

含める内容：

```cpp
WIFI_SSID
WIFI_PASSWORD

MQTT_BROKER
MQTT_PORT
MQTT_USER
MQTT_PASSWORD

MQTT_TOPIC_LIVING
MQTT_TOPIC_ENTRANCE

THI_OPEN_DIFF
THI_CLOSE_DIFF
```

---

# 使用ライブラリ

Arduino Library Manager：

```text
M5StickCPlus2
PubSubClient
ArduinoJson
```

---

# Board設定

```text
M5Stick-C Plus2
```

M5Stack ESP32 core 使用。

---

# VSCode / Arduino環境メモ

## 注意点

空 `.ino` では VSCode Arduino 拡張が失敗する。

最低限：

```cpp
void setup() {}
void loop() {}
```

が必要。

---

# 現在の問題

## Arduino libraries 汚染

以下フォルダが壊れている可能性：

```text
~/Documents/Arduino/libraries/
```

警告例：

```text
invalid library: no header files found
```

今後の事故要因になりうる。

---

# 今後の改善候補

優先順位順。

---

## 1. MQTT timeout監視

一定時間受信なし：

```text
ERROR
```

表示。

---

## 2. HOLD状態

差が小さい場合：

```text
HOLD
```

を追加。

---

## 3. 平均THI化

現在：

```text
瞬間値
```

将来：

```text
5〜10分移動平均
```

へ変更。

---

## 4. 矢印UI

例：

```text
OPEN ↑
```

---

## 5. ブザー通知

状態遷移時のみ。

---

## 6. DeepSleep

低消費電力化。

---

# このプロジェクトの本質

これは、

```text
環境表示
```

ではない。

本質は：

```text
人間行動を1秒で決定させるUI
```

である。

情報量を増やしすぎると失敗する。

---

# 命名

正式名称：

```text
STICKCPLUS2-THI-AirGate
```

---

# 開発方針

優先順位：

1. 安定動作
2. 誤判定削減
3. 視認性
4. MQTT耐障害性
5. 低消費電力

「多機能化」は後回し。

---

# 推奨デバッグ手順

## MQTT確認

macOS / Linux：

```bash
mosquitto_sub -h 192.168.3.200 -u "$MQTT_USER" -P "$MQTT_PASSWORD" -t 'home/#' -v
```

---

## THI確認

```bash
mosquitto_sub -h 192.168.3.200 -u "$MQTT_USER" -P "$MQTT_PASSWORD" \
-t 'home/co2/aggregate/raw' \
-t 'home/env/env4-entrance/raw' -v
```

---

# 将来的な発展

この設計は、

```text
空気の流れを数値で制御する
```

方向へ発展可能。

将来的には：

- AIR SCORE
- 換気価値
- 外気流入効率
- エアコン負荷削減量

などへ拡張可能。

---

# さらに

「ドア開放後のTHI改善速度」を測定できれば、“本当に効果があった換気”だけを学習するシステムへ進化できます。
