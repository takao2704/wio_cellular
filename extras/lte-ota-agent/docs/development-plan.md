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
| M5 | 障害耐性 | 通信断、電源断、破損、容量超過を試験 | 完了（自動試験と実機A1〜A6・B1〜B3・C1〜C2完了） |
| M6 | セキュリティ・運用 | 署名、アンチロールバック、段階配信 | 完了（自動試験・HW v1.0実機1台での受入試験1〜8） |

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

初期PoCの`lte_bootstrap`は、HTTPレスポンスの`Content-Length`を必須とし、chunked transferには対応しない。manifestの`size`、HTTPレスポンスの`Content-Length`、Bank 1へ書き込んだバイト数を照合するためである。HTTPSも初期PoCの対象外とし、SORACOM AirからSORACOMサービスへ接続するHTTP経路を前提とした。

初期の実機切り分けでは、DNS解決を検証対象から外すため、MetadataとHarvest FilesをIPアドレスで指定した。これはDNS障害が実測されたためではなく、HTTP取得、UFS保存、Bank 1書込みの検証に集中するための一時的な設定である。エンドポイントのIPアドレスを製品仕様として固定する意図はなく、現在の再現用サンプルでは`metadata.soracom.io`と`harvest-files.soracom.io`を使用し、manifest内のファームウェアURLも許可したFQDNとポートに一致する場合だけ受け付ける。SORACOMサービスのIPアドレスは将来変更される可能性があるため、恒久的な設定にはFQDNを使用する。

`include/ota_config.local.h.example`は初期PoC用の設定例である。誤って遠隔更新を開始しないよう、`WIO_OTA_ENABLED`と`WIO_OTA_AUTO_APPLY`の初期値は無効とし、検証段階に応じて明示的に有効化する。この安全設定は、DNSやHTTP方式の選択とは独立している。

Metadata ServiceとHarvest Filesは対象IoT SIMのグループで有効化する。Metadataはreadonlyのままでよく、ユーザーデータにmanifest JSONを設定する。Harvest Filesにはバージョン別の不変パス（例: `/wio-bg770a/v2/firmware.bin`）でraw binaryを保存する。

2026-08-30のHW v1.0実機試験では、Metadataから244-byteのmanifestをHTTP 200で取得し、Harvest Filesから46,876-byteのv2イメージをHTTP 200で取得した。HTTP本文をUARTへ一括出力する`QHTTPREAD`では32 KiB通過後に本文タイムアウトが発生したため、`QHTTPREADFILE`でUFSへ一時保存し、`QFREAD`で512-byteずつ読み出す方式へ変更した。この方式では46,876 bytesをBank 1へ書き込み、CRC16 `0x0843`とSHA-256を検証し、`ota-check-complete`を確認した。自動適用は無効のままとし、M3ではBank 1への検証済み保存までを完了条件とした。

## M4の実機結果

更新後もOTA機能を失わないよう、`lte_bootstrap`を基に`WIO_OTA_CURRENT_VERSION=3`を指定した`lte_target_v3`を作成した。配信イメージは103,988 bytes、CRC16 `0x235b`、SHA-256 `b6c56f8dee6fd86aa10837c752a725fac722938770def1e1a8bd592c669ac950`である。Harvest Filesへ配置して再取得時のSHA-256一致を確認し、Metadataのmanifestをversion 3へ切り替えた。

USB DFUで一度だけ`lte_bootstrap_apply`を書き込んだ後は、LTEだけでmanifest確認、firmware取得、Bank 1書込み、CRC16/SHA-256検証、有効化、再起動まで完了した。bootloader反映後のv3は同じmanifestを再取得し、`[OTA] no update current=3 manifest=3`および`ota-check-complete`を出力した。これにより、更新後もOTAエージェントが残るLTE OTA E2Eを確認した。

## ユーザーアプリ向けライブラリ化

M4で実機確認したmanifest取得、技術的検証、Harvest Files download、Bank 1検証、適用処理を`WioOtaAgent` 0.1.0へ抽出した。更新の有無と適用可否は`DecisionCallback`でユーザーアプリが判断し、`kNoUpdate`、`kDefer`、`kDownloadAndVerify`、`kApply`、`kReject`を返す。これにより、`WIO_OTA_CURRENT_VERSION`はライブラリ必須設定ではなくサンプルアプリの判断材料になった。

`WioOtaAgent`はLTE attachやPSM復帰を行わず、利用可能なPDP contextを前提に1回分のOTA確認を同期実行する。日次スケジュール、バッテリー条件、通常処理の一時停止、失敗時の再試行はユーザーアプリの責務とする。詳細は[WioOtaAgent組み込みガイド](ota-library-integration.md)を参照する。ライブラリ化後のビルドはFlash約132KBで、397,312-byte制限内に収まっている。

2026-08-30の追加実機試験では、旧v3が自動適用無効だったため、ライブラリ化した`lte_target_v3_apply`をUSB DFUで試験起点として導入した。以後はLTEだけで131,784-byteのv4を取得し、CRC16 `0xbde9`、SHA-256 `7c51d4f6976c844a05ad5dbc268dd0be05e42b601456838dca8743b7842d87ae`を検証して適用した。再起動後のv4で`current=4 manifest=4`、`application reports no update`、`ota-check-complete`を確認した。v4も自動適用を有効にしているため、同じAgentによる将来のv5更新経路を保持している。

## M5の進捗

manifest検証を`WioOtaManifest`へ、受信サイズとCRC16/SHA-256検証を
`WioOta::ImageVerifier`へ分離した。これにより、実機フラッシュを操作せずに
次の失敗を自動試験できる。

- HW識別子、サイズ、CRC16、SHA-256が不正なmanifest
- HTTPS、許可外ホスト、許可外ポートを指定したURL
- ファームウェア受信の途中切断
- CRC16不一致、SHA-256不一致、受信サイズ超過

自動試験は`tools/run_native_tests.sh`で実行する。実機の通信断と電源断は
[M5障害注入試験](m5-fault-injection.md)のA1〜A6、B1〜B3、C1〜C2を完了した。manifest不正、
HTTP応答不一致、CRC16不一致、LTE session断、Wio本体reset、装置全体の電源断で、
現在のアプリが起動不能にならないことを実機で確認した。さらにM5専用停止ポイントで、
検証完了後のsettings登録前resetと、settings登録後のsoftware reset前resetを確認した。
Bank 1からBank 0へのcopy中に電源を切るC3は通常の受入対象から除外し、SWD復旧手段を
用意した破壊的試験として別管理する。

## セキュリティ上の注意

format 2 manifest、Ed25519署名、最高適用versionの二重化保存、端末ごとの
決定的bucketによる段階配信を実装した。詳細と鍵管理手順は
[M6 署名・アンチロールバック・段階配信](m6-security-and-rollout.md)を参照する。
2026-08-31に、未署名manifestの拒否、署名付きv2→v3のLTE OTA、更新後の同一versionに
対する更新なし判定、署名改ざんと旧versionの拒否、配信率0%での延期を実機で確認した。
その後、配信率を100%へ変更してv3→v4更新を確認した。v4起動時に保存済みversion 3の
再読出しとversion 4の保存に成功し、v4をもう一度resetした後も`loaded=4 current=4 highest=4`
を確認した。M6の予定した受入試験は完了した。詳細と未検証範囲は
[M6実機検証記録](m6-hardware-validation-2026-08-31.md)を参照する。
通常ビルドとの互換性のためformat 1は残しているが、本番では
`require_signature = true`を必須とする。秘密鍵、配信URL、APNなどの実値は
リポジトリへコミットしない。
