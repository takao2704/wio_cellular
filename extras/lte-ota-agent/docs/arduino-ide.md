# Arduino IDEでCellularStatusOtaを使う

PlatformIO版と同じLTE OTA処理を、Arduino IDE用の
[`CellularStatusOta.ino`](../examples/cellular-status-ota/CellularStatusOta/CellularStatusOta.ino)
から利用できる。PlatformIOの`src/main.cpp`もこのファイルを読み込むため、
アプリ本体の実装は共通である。

このスケッチは起動時にLTE接続し、SORACOM Metadataから更新情報を1回取得する。
署名検証・アンチロールバック・段階配信の条件を満たした新しいversionがあれば、
Harvest Filesから取得して自動適用・再起動する。更新がなければ5秒ごとに稼働ログを出す。
日次確認は自動ではなく、[組み込みガイド](ota-library-integration.md#1日1回確認する例)
のようにアプリ側で実行タイミングを決める。

## 前提

- Wio BG770A **HW v1.0**、dual-bank bootloader、SoftDevice S140 7.3.0
- Arduino IDE 2系、SeeedJP nRF52 Boards **1.5.1**
- WioCellular **0.3.15**、ArduinoJson **7.0.4**（GUIのUSB導入試験では**7.4.3**も確認）
- SORACOM Air経由でMetadataとHarvest Filesへ接続できるSIM
- PCにPython 3と、署名ツール用の`cryptography`パッケージ

通常のスケッチ書込みではbootloaderは更新されない。未移行の実機では先に
[bootloader確認・移行手順](bootloader-migration.md)を確認する。
ボードパッケージをPCに入れただけでは、実機の移行が完了したことにはならない。
アプリ上限は397,312 bytesである。

以降の端末コマンドは、このOTAプロジェクトのルートで実行する。
`wio_cellular`のforkを取得した場合は`extras/lte-ota-agent`が該当する。

## 1. ボードとライブラリを導入する

Arduino IDEの「設定」にある「追加のボードマネージャのURL」へ、次を追加する。
既存のURLは残す。

```text
https://www.seeed.co.jp/package_SeeedJP_index.json
```

ボードマネージャでSeeedJPのnRF52ボードパッケージ1.5.1を導入し、
ライブラリマネージャでWioCellular 0.3.15とArduinoJson 7.0.4を導入する。
導入画面の基本操作は[Seeed公式マニュアル](https://seeedjp.github.io/Wiki/Wio_BG770A/user-manual.html)
を参照する。既存プロジェクトで別versionを使っている場合は、変更の影響を確認してから切り替える。

OTA用の3ライブラリは、このリポジトリからZIPを作る。
リポジトリ全体のZIPをArduino IDEに読み込ませる形式ではない。

```bash
python3 tools/package_arduino_libraries.py
```

Arduino IDEの「スケッチ」→「ライブラリをインクルード」→「.ZIP形式のライブラリをインストール」
で、次の順に選択する。

1. `dist/arduino/WioOta.zip`
2. `dist/arduino/WioBg770aHttp.zip`
3. `dist/arduino/WioOtaAgent.zip`

すでに同名ライブラリがある場合は、保存先とversionを確認し、必要なバックアップを取ってから
入れ替える。古いコピーを残して二重に読み込ませない。

ZIPを使わず、`lib/WioOta`、`lib/WioBg770aHttp`、`lib/WioOtaAgent`の各フォルダを
スケッチブックの`libraries`直下へコピーしてもよい。Arduino IDEではPlatformIOの
`lib_extra_dirs`や`lib_archive`の設定は不要である。

## 2. スケッチと公開鍵を用意する

「ファイル」→「スケッチ例」→「WioOtaAgent」→「CellularStatusOta」を開き、
編集用のコピーをスケッチブックへ保存する。またはリポジトリの
`examples/cellular-status-ota/CellularStatusOta/CellularStatusOta.ino`を直接開く。
以下のコマンドは後者のパスで示す。コピーした場合は`--output`の出力先も合わせる。

スケッチの`ota_sketch_config.h`タブでversionを指定する。

```cpp
#ifndef APP_VERSION
#define APP_VERSION 1
#endif

#ifndef WIO_OTA_ARDUINO_SECURE
#define WIO_OTA_ARDUINO_SECURE 1
#endif
```

署名検証は既定で有効であり、公開鍵ヘッダがない状態ではビルドを失敗させる。
配布ZIPに仮の鍵や手元の検証鍵は含めていない。

すでに署名付きOTAを運用している場合は、その公開鍵と`key_id`を使う。
新規の検証端末向けに鍵を作る場合だけ、次の例を使う。秘密鍵はスケッチへコピーしない。

```bash
python3 -m venv .venv
.venv/bin/python -m pip install cryptography

# 新規検証用。既存の運用鍵は作り直さない。
ota_key_dir="$(mktemp -d "$HOME/wio-ota-keys.XXXXXX")"
openssl genpkey -algorithm ED25519 -out "$ota_key_dir/signing-key.pem"
chmod 600 "$ota_key_dir/signing-key.pem"
openssl pkey -in "$ota_key_dir/signing-key.pem" -pubout \
  -out "$ota_key_dir/public-key.pem"

.venv/bin/python tools/export_manifest_public_key.py "$ota_key_dir/public-key.pem" \
  --key-id demo-2026 \
  --output examples/cellular-status-ota/CellularStatusOta/ota_manifest_public_key.h
```

`ota_key_dir`の保存先を控え、以後の更新でも同じ鍵を使う。上記の一時変数は同じ端末セッション
内でのみ有効である。本番鍵の管理と鍵更新は[署名・鍵管理の手順](security-and-rollout.md)を参照する。

スケッチフォルダは次の構成になる。

```text
CellularStatusOta/
├── CellularStatusOta.ino
├── ota_sketch_config.h
└── ota_manifest_public_key.h  # 公開鍵から生成
```

このサンプルはversionで新旧を比較するため`APP_VERSION`を使うが、ライブラリ一般の
必須ビルドフラグではない。独自の判定方式は`DecisionCallback`へ実装できる。
ただしアンチロールバックを使う場合は、署名するversionと現在のversionを正しく管理する。

## 3. 検証し、最初のアプリをUSBで書き込む

「ツール」で次を選択する。

| 項目 | 設定 |
|---|---|
| ボード | Wio BG770A |
| Board Version | 1.0 |
| SoftDevice | S140 7.3.0 |
| Print Port | Serial (USB-CDC) |
| ポート | 接続したWio BG770Aのポート |

まず「検証・コンパイル」を実行する。HW v1.1を選んだ場合は、このHW v1.0専用サンプルは
エラーになる。Arduino IDE/CLIでは署名検証実装をライブラリ側でも自動的にコンパイルするため、
追加のコンパイラフラグは不要である。署名を必須にする動作はスケッチの
`config.security.require_signature = true`で設定している。

「書き込み」を行うと既存アプリを置き換える。先にSIMグループのMetadataユーザーデータを確認し、
意図しない更新が配信されていないことを確認する。このスケッチは起動直後にOTAを確認する。
USB書込み時にDFUへ入れない場合だけ、RESETをダブルクリックしてポートを選び直す。
この段階で「ブートローダを書き込む」は実行しない。

シリアルモニタを115200 bpsで開く。新規のversion 1端末では
`cellular-status + OTA, app-version=1`とLTE接続後の状態ログが確認点になる。
通信失敗や更新なしの場合にも、その結果を出力する。

すでに運用中の端末なら、新しいビルドは最高適用versionを超える値にする。
version 1からの例をそのまま既存端末へ適用したり、更新を通すために不揮発レコードを消したりしない。

## 4. 次のversionをビルドして配信ファイルを作る

新規のversion 1端末から更新する例では、`ota_sketch_config.h`の`APP_VERSION`を2に変更する。
公開鍵・`key_id`・署名必須設定は引き継ぐ。この更新先をUSBで書き込む必要はない。

「スケッチ」→「コンパイル済みのバイナリをエクスポート」を実行する。
SeeedJP core 1.5.1では、スケッチフォルダ配下に次のDFU ZIPが生成される。

```text
build/SeeedJP.nrf52.wio_bg770a/CellularStatusOta.ino.zip
```

同時に生成される`.hex`や`.elf`ではなく、**application用の`.zip`**を次のツールへ渡す。
ZIP内部の`manifest.json`を読み、`CellularStatusOta.ino.bin`を取り出す。
bootloaderやSoftDeviceを含むDFU ZIPは受け付けない。

```bash
.venv/bin/python tools/firmware_manifest.py \
  examples/cellular-status-ota/CellularStatusOta/build/SeeedJP.nrf52.wio_bg770a/CellularStatusOta.ino.zip \
  --version 2 \
  --url http://harvest-files.soracom.io/wio-bg770a/arduino-v2/firmware.bin \
  --signing-key "$ota_key_dir/signing-key.pem" \
  --key-id demo-2026 \
  --release-id arduino-v2 \
  --rollout 10000 \
  --output dist/arduino-v2/manifest.json \
  --firmware-output dist/arduino-v2/firmware.bin
```

`--version`はビルドした`APP_VERSION`に合わせる。ツールはバイナリからアプリversionを
自動取得しない。ビルド後に設定だけ書き換え、古いZIPへ新versionを付けないこと。
DFU ZIP内の`application_version`はこのOTA用のversionではない。

## 5. SORACOMから配信して確認する

対象SIMのグループでMetadataを有効・read-onlyにし、Harvest Filesを有効にする。
OTA実行中は通信できる状態を維持し、PSMからの復帰はアプリ側で管理する。
サービス利用料金が発生するため、試験対象のSIM・グループ・配信先を確認してから操作する。

1. 生成した`dist/arduino-v2/firmware.bin`をHarvest Filesの
   `/wio-bg770a/arduino-v2/firmware.bin`へアップロードする。DFU ZIPではなくraw binaryを使う。
2. その後で、同じ生成処理で作った`manifest.json`の内容をMetadataユーザーデータへ設定する。
   URL・version・rolloutなどを変更する場合も、JSONだけを編集せず署名ツールで作り直す。
3. version 1の実機を通常リセットする。DFUモードにはしない。
4. ダウンロード進捗、検証成功、再起動、`app-version=2`、同じmanifestに対する
   `no update`を確認する。

このスケッチの取得先は`http://metadata.soracom.io/v1/userdata`で、
firmwareに許可するホストは`harvest-files.soracom.io`、ポートは80である。
manifestへ別ホストやHTTPS URLを書いても受け付けない。

## 検証範囲

PlatformIOとArduino CLIでは、実機で新しいアプリへのLTE OTAと更新後の「更新なし」
判定を確認した。Arduino IDE GUIではコンパイル、USB書込み、LTE接続、署名付きmanifestの
更新なし判定、バイナリエクスポートを確認した。GUI生成バイナリ同士のLTE OTAは未試験である。
環境、ログ、未検証項目はforkの固定コミットにある[検証記録](https://github.com/takao2704/wio_cellular/blob/873ded2438b8083a7f001cde241c6e2962187a54/extras/lte-ota-agent/docs/validation/README.md)を参照できる。

## Arduino CLI

Arduino CLIで同じスケッチをビルド・エクスポートする場合は次を使う。
前述のボード、ライブラリ、公開鍵をCLIが参照する環境にも導入しておく。

```bash
arduino-cli compile \
  --fqbn SeeedJP:nrf52:wio_bg770a:board_version=1_0,softdevice=s140v7,debug_output=serial \
  --export-binaries examples/cellular-status-ota/CellularStatusOta
```

Arduino IDEでは署名検証実装を常に利用可能にするため、明示的に
`WIO_OTA_ARDUINO_SECURE=0`とした旧format 1検証用構成でも、署名機能を除外した
PlatformIOビルドよりサイズが増える。通常利用では既定の1を維持する。
