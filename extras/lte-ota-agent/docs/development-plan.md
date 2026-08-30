# Wio BG770A HW v1.0 LTE OTA 開発計画

## 目標

HW v1.0、bootloader v1.0、既存ユーザーアプリが動作している実機を、一度だけUSB経由で移行し、以後はLTE経由でユーザーアプリを更新できるようにする。

## 確定した基盤

- Arduino Core: SeeedJP `Adafruit_nRF52_Arduino` 1.5.1 (`514f28e`)
- WioCellular: 0.3.15 (`8a354c2`)
- Bootloader source: SeeedJP `Adafruit_nRF52_Bootloader` (`3cb5c65`)
- SoftDevice: S140 7.3.0
- HW build option: `BOARD_VERSION_1_0`
- Bank 0: `0x27000`
- Bank 1: `0x88000`
- 最大アプリサイズ: `397,312 bytes` (`0x61000`)
- Bootloader: `0xF4000`
- Bootloader settings: `0xFF000`
- 公式bootloader + S140 ZIP SHA-256: `ba308f0f028706225764719d470b6facd7bd5e643f11b1781e5aa6a09106b8a5`

PlatformIOの公式board定義は最大サイズが`798,720 bytes`のままなので、本リポジトリでは`board_upload.maximum_size = 397312`で上書きする。

## マイルストン

| ID | 内容 | 完了条件 | 状態 |
|---|---|---|---|
| M0 | 現行公式ソース、HW v1.0設定、メモリマップを固定 | 再現可能なビルド設定と根拠が残る | 完了 |
| M1 | USB DFUでbootloader + S140 7.3.0へ移行し、bootstrapを書き込む | bootstrap起動とUSBログを確認 | 完了 |
| M2 | 通信非依存のBank 1 writerとローカル転送PoC | USBからv2を受信し、CRC検証後にv1→v2更新 | 完了 |
| M3 | LTE HTTPストリーミング | manifestとbinをLTEで取得しBank 1へ保存 | 完了 |
| M4 | LTE OTA E2E | 遠隔更新確認から更新結果通知まで成功 | 完了 |
| M5 | 障害耐性 | 通信断、電源断、破損、容量超過を試験 | 未着手 |
| M6 | セキュリティ・運用 | 署名、アンチロールバック、段階配信 | 未着手 |

## M1の実機手順

1. 現行アプリのバージョンとUSB認識を記録する。
2. Arduino IDEで`Board Version = 1.0`、`SoftDevice = S140 7.3.0`、`Programmer = Bootloader DFU for Bluefruit nRF52`を選ぶ。
3. 「ブートローダを書き込む」で公式のbootloader + SoftDeviceパッケージを転送する。
4. この処理では現行アプリ領域が消えるため、続けて`bootstrap`をUSBで書き込む。
5. `WIO OTA bootstrap v0.1`が115200 bpsで出力されることを確認する。

USB DFUに失敗した場合だけSWD/J-Linkを復旧手段として使う。M1は実機の既存アプリを消去するため、明示的な実施承認なしには行わない。

## M2の更新フロー

1. host toolがraw `firmware.bin`のサイズ、Nordic互換CRC16、SHA-256を送る。
2. bootstrapがBank 1の必要ページだけを消去する。
3. 512-byte単位で受信し、書込み直後にread-back比較する。
4. 全体CRC16、SHA-256、vector tableを検証する。
5. hostから明示的に`APPLY`を受けた場合だけbootloader settingsを更新する。
6. 再起動後、bootloaderがBank 1をBank 0へコピーする。

`APPLY`前のリセットでは更新は有効化されない。settings page更新中の電源断は現行bootloaderの単一settings page設計上の弱点であり、M5で必ず試験する。

## M2実機試験コマンド

```bash
pio run -e bootstrap
pio run -e blinky_v2
pio run -e bootstrap -t upload
python3 tools/send_firmware.py /dev/cu.usbmodemXXX .pio/build/blinky_v2/firmware.zip
python3 tools/send_firmware.py /dev/cu.usbmodemXXX .pio/build/blinky_v2/firmware.zip --apply
```

最初の転送は`--apply`なしでBank 1への書込みと検証だけを行う。結果を確認後、`--apply`付きで更新を有効化する。

2026-08-30のHW v1.0実機試験では、46,876-byteのv2イメージについてCRC16 `0x0843`、SHA-256 `19f4d2e85f80c14e3771cc43ad8bfefeddeb7749065047eeeb9d4d7c2aacf0ae`が一致した。検証のみの転送は`ABORT`で未適用に戻り、続く`APPLY`付き転送ではbootloader settings更新と再起動後の通常アプリ復帰を確認した。詳細は[実機検証記録](hardware-validation-2026-08-30.md)を参照する。

## M3の実装

- BG770A内蔵HTTPクライアント（`QHTTPCFG`、`QHTTPURL`、`QHTTPGET`）
- 大きなHTTP応答をモデム内UFSへ保存する`QHTTPREADFILE`
- `QFOPEN`、`QFREAD`による512-byte単位のBank 1転送
- 固定Content-Lengthの必須化（chunked transferは初期PoCでは不採用）
- manifestの`hardware`、`version`、`size`、`crc16`、`sha256`検証
- 通信断時の再試行。途中再開はM5で設計する
- manifestはSORACOM Metadataのユーザーデータから取得する
- `firmware.bin`はSORACOM Harvest Filesから取得する

`lte_bootstrap`は固定Content-Lengthのみを受け付け、chunked transferとHTTPS URLを拒否する。manifestはMetadataの閉域IP `http://100.127.100.127/v1/userdata`、ファームウェアはHarvest Filesの閉域IP `http://100.127.111.48/...`を使い、manifest内のURLもHarvest FilesのIPと80番ポートに限定する。DNS依存を避けるため、SORACOM公式の固定サービスIPを利用する。`include/ota_config.local.h.example`をコピーして設定する。既定では`WIO_OTA_ENABLED=0`かつ`WIO_OTA_AUTO_APPLY=0`であり、誤って遠隔更新を開始しない。

Metadata ServiceとHarvest Filesは対象IoT SIMのグループで有効化する。Metadataはreadonlyのままでよく、ユーザーデータにmanifest JSONを設定する。Harvest Filesにはバージョン別の不変パス（例: `/wio-bg770a/v2/firmware.bin`）でraw binaryを保存する。

2026-08-30のHW v1.0実機試験では、Metadataから244-byteのmanifestをHTTP 200で取得し、Harvest Filesから46,876-byteのv2イメージをHTTP 200で取得した。HTTP本文をUARTへ一括出力する`QHTTPREAD`では32 KiB通過後に本文タイムアウトが発生したため、`QHTTPREADFILE`でUFSへ一時保存し、`QFREAD`で512-byteずつ読み出す方式へ変更した。この方式では46,876 bytesをBank 1へ書き込み、CRC16 `0x0843`とSHA-256を検証し、`ota-check-complete`を確認した。自動適用は無効のままとし、M3ではBank 1への検証済み保存までを完了条件とした。

## M4の実機結果

更新後もOTA機能を失わないよう、`lte_bootstrap`を基に`WIO_OTA_CURRENT_VERSION=3`を指定した`lte_target_v3`を作成した。配信イメージは103,988 bytes、CRC16 `0x235b`、SHA-256 `b6c56f8dee6fd86aa10837c752a725fac722938770def1e1a8bd592c669ac950`である。Harvest Filesへ配置して再取得時のSHA-256一致を確認し、Metadataのmanifestをversion 3へ切り替えた。

USB DFUで一度だけ`lte_bootstrap_apply`を書き込んだ後は、LTEだけでmanifest確認、firmware取得、Bank 1書込み、CRC16/SHA-256検証、有効化、再起動まで完了した。bootloader反映後のv3は同じmanifestを再取得し、`[OTA] no update current=3 manifest=3`および`ota-check-complete`を出力した。これにより、更新後もOTAエージェントが残るLTE OTA E2Eを確認した。

## ユーザーアプリ向けライブラリ化

M4で実機確認したmanifest取得、技術的検証、Harvest Files download、Bank 1検証、適用処理を`WioOtaAgent` 0.1.0へ抽出した。更新の有無と適用可否は`DecisionCallback`でユーザーアプリが判断し、`kNoUpdate`、`kDefer`、`kDownloadAndVerify`、`kApply`、`kReject`を返す。これにより、`WIO_OTA_CURRENT_VERSION`はライブラリ必須設定ではなくサンプルアプリの判断材料になった。

`WioOtaAgent`はLTE attachやPSM復帰を行わず、利用可能なPDP contextを前提に1回分のOTA確認を同期実行する。日次スケジュール、バッテリー条件、通常処理の一時停止、失敗時の再試行はユーザーアプリの責務とする。詳細は[WioOtaAgent組み込みガイド](ota-library-integration.md)を参照する。ライブラリ化後のビルドはFlash約132KBで、397,312-byte制限内に収まっている。

2026-08-30の追加実機試験では、旧v3が自動適用無効だったため、ライブラリ化した`lte_target_v3_apply`をUSB DFUで試験起点として導入した。以後はLTEだけで131,784-byteのv4を取得し、CRC16 `0xbde9`、SHA-256 `7c51d4f6976c844a05ad5dbc268dd0be05e42b601456838dca8743b7842d87ae`を検証して適用した。再起動後のv4で`current=4 manifest=4`、`application reports no update`、`ota-check-complete`を確認した。v4も自動適用を有効にしているため、同じAgentによる将来のv5更新経路を保持している。

## セキュリティ上の注意

CRC16とSHA-256だけでは配信者の真正性を保証できない。本番投入前に署名検証を必須化する。署名鍵、配信URL、APNなどの実値はリポジトリへコミットしない。
