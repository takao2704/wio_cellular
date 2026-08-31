# Wio BG770A LTE OTA

既存のWio BG770AユーザーアプリへOTA処理を追加するライブラリと利用例です。
アプリが更新を確認するタイミングと適用可否を決め、ライブラリが取得・検証・更新登録を担当します。
付属サンプルではSORACOM Metadataからmanifestを取得し、Harvest Filesからアプリを取得します。

対象はHW v1.0、対応dual-bank bootloader、SoftDevice S140 7.3.0です。
アプリの上限は397,312 bytesです。PCのボードパッケージ導入だけでは、実機のbootloaderは更新されません。

## 導入

- [bootloaderの確認・移行](docs/bootloader-migration.md)
- [PlatformIOでビルドしてOTAする](docs/platformio.md)
- [Arduino IDE／CLIでビルドしてOTAする](docs/arduino-ide.md)
- [既存アプリへの組み込みと日次確認](docs/ota-library-integration.md)
- [署名・アンチロールバック・段階配信](docs/security-and-rollout.md)

PlatformIOとArduino IDEは、同じ
[CellularStatusOta.ino](examples/cellular-status-ota/CellularStatusOta/CellularStatusOta.ino)
を使います。初回はOTA処理を含むアプリをUSBで書き込み、次のアプリからLTEで配信します。
更新後のアプリにもOTA処理と公開鍵を残してください。

## 運用

1. 配信側で更新アプリをビルドし、付属ツールでmanifestへ署名します。秘密鍵は端末に入れません。
2. raw binaryをリリースごとの上書きしないHarvest Filesのパスへ配置し、内容を確認します。
3. 対象SIMグループを確認してから、Metadataユーザーデータを対応するmanifestへ切り替えます。
4. 端末のアプリが通信可能な時間に更新を確認します。電池残量や処理状況による延期はアプリが判断します。
5. 検証と適用登録の後、bootloaderが更新アプリを反映します。更新結果の遠隔通知はアプリ側へ実装します。

サンプルは起動時に1回確認します。1日1回などのスケジュール、通信失敗時の再試行、
PSM復帰はアプリ側で管理します。配信側から端末を即時に起こす仕組みはありません。
署名必須・アンチロールバック・段階配信の設定を確認してから運用してください。

## ファイルの役割

| 場所 | 用途 |
|---|---|
| `lib/WioOta` | Bank 1書込み、イメージ検証、bootloaderへの登録 |
| `lib/WioBg770aHttp` | BG770AのHTTP取得とUFS読み出し |
| `lib/WioOtaAgent` | manifest検証と更新処理の制御、署名・配信対象判定、バージョン保存 |
| `examples/cellular-status-ota` | PlatformIO／Arduinoで共通の利用例 |
| `tools/firmware_manifest.py`、`firmware_utils.py` | 配信用raw binaryの抽出とmanifest生成・署名 |
| `tools/export_manifest_public_key.py` | 公開鍵のC++ヘッダ生成 |
| `tools/package_arduino_libraries.py` | Arduino IDE用ZIPの生成 |
| `tests/native`、`tests/python` | 回帰テスト |
| `tests/hardware` | 保守担当者向けの停止・リセット試験設定 |
| `docs/validation` | 実測ログと確認範囲 |

初期のUSB転送PoC、LTE接続診断アプリ、開発計画は
[整理前のコミット](https://github.com/takao2704/wio_cellular/tree/de0044f74c540a4258d631c5ee6499d431f13840/extras/lte-ota-agent)
から参照できます。通常の導入には使いません。

## テストと制約

[PC上の回帰テスト](tests/README.md)と[実機障害試験](docs/fault-injection.md)を分けています。
[実機検証の一覧](docs/validation/README.md)には、PlatformIO／Arduino CLIでのLTE OTAと、
Arduino IDE GUIでのコンパイル・USB書込み・更新なし判定の記録があります。
GUI生成バイナリ同士のLTE OTAは未試験です。

通信はHTTP・固定Content-Lengthが前提です。HTTPS、chunked transfer、途中再開には未対応です。
署名検証はAgent側の機能であり、bootloaderのsecure bootではありません。
更新後に起動しないアプリからの自動ロールバックもありません。
実機検証はHW v1.0の1台で行っており、HW v1.1、多台数の段階配信、
bootloader反映中や不揮発レコード書込み中の電源断は未検証です。
