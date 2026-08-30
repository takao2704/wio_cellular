# HW v1.0 実機検証記録（2026-08-30）

## 対象

- Board: Seeed Wio BG770A HW v1.0
- USB serial: `<DEVICE_SERIAL>`
- Application mode VID/PID: `2886:8056`
- DFU mode VID/PID: `2886:0056`

## M1: 基盤移行

DFUモードで確認した移行前の情報:

- Bootloader: `0.9.1-19-g4ed6708-dirty`
- SoftDevice: S140 7.3.0

SHA-256 `ba308f0f028706225764719d470b6facd7bd5e643f11b1781e5aa6a09106b8a5`と照合した公式bootloader + S140 ZIPをSerial DFUで書き込んだ。書込み後の`INFO_UF2.TXT`は次の情報を返した。

- Bootloader: `0.9.1-33-g3cb5c65-dirty`
- SoftDevice: S140 7.3.0
- Build date: Jul 8 2025

続いてHW v1.0向けbootstrapをapplication DFUで書き込み、USB Serialでプロトコル応答を確認した。

## M2: Bank 1更新

更新対象:

- Image: `blinky_v2`
- Size: 46,876 bytes
- CRC16-CCITT: `0x0843`
- SHA-256: `19f4d2e85f80c14e3771cc43ad8bfefeddeb7749065047eeeb9d4d7c2aacf0ae`

検証のみの転送結果:

```text
READY 46876
PROGRESS 16384/46876
PROGRESS 32768/46876
PROGRESS 46876/46876
VERIFIED crc16=843
SEND APPLY TO COMMIT, OR RESET TO ABORT
ABORTED not committed
```

同じイメージを再送して`APPLY`した結果:

```text
VERIFIED crc16=843
SEND APPLY TO COMMIT, OR RESET TO ABORT
ACTIVATED rebooting
```

再起動後はapplication mode `2886:8056`へ復帰し、旧bootstrapの識別コマンドには応答しなくなった。これにより、検証済みv2イメージのBank 1登録、bootloaderによる反映、旧bootstrapからの置換を確認した。

## 試験中に修正した不具合

USB転送プロトコルのヘッダーはSHA-256を含めて82文字だが、bootstrapのコマンドバッファが64文字だったため途中で切れていた。バッファを96文字へ拡張した。また、検証のみの転送後にwriterをidleへ戻す`discard()`と、host側から明示的に`ABORT`する処理を追加した。

## M3: LTE経由の取得とBank 1検証

配信元は次のSORACOMサービスを使用した。

- manifest: Metadata Serviceのユーザーデータ
- firmware: Harvest Filesのバージョン別パス

対象SIMのグループでMetadata（read-only）とHarvest Filesを有効化し、Globalカバレッジ側へM2と同じ46,876-byteのraw firmwareを配置した。アップロード後に再取得してSHA-256が一致することを確認した。

実機ではLTE attach、PDP activation、SORACOM閉域pingに成功し、次のHTTP応答を得た。

```text
[HTTP] manifest status=200 length=244
[OTA] downloading version=2 size=46876
[HTTP] firmware status=200 length=46876
```

最初に試した`QHTTPREAD`によるUARTへの本文一括出力では、32 KiB通過後に本文タイムアウトとなった。WioCellularが使用するUART受信リングは64 bytesであり、受信とnRF52840内蔵Flash書込みを同時に行う構成では余裕がないため、HTTP応答をBG770A内UFSへ保存してから読み出す方式へ変更した。

採用したフロー:

```text
QHTTPGET
  -> QHTTPREADFILE (UFSへ保存)
  -> QFOPEN
  -> QFREAD (512 bytesずつ)
  -> Bank 1へ書込み・read-back比較
  -> QFCLOSE / QFDEL
  -> CRC16 / SHA-256 / vector table検証
```

実機結果:

```text
[HTTP] UFS response stored
[HTTP] UFS file opened handle=0
[HTTP] UFS first read payload=512
[OTA] progress 16384/46876
[OTA] progress 32768/46876
[OTA] progress 46876/46876
[OTA] image verified
[OTA] verified only; WIO_OTA_AUTO_APPLY is disabled
[STATUS] phase=ota-check-complete sim=...<LAST4>
```

これにより、SORACOM Metadataで更新情報を取得し、Harvest FilesからLTE経由でraw firmwareを取得してBank 1へ保存し、M2と同じCRC16 `0x0843`およびSHA-256を検証するM3の完了条件を満たした。自動適用は無効のため、この試験ではBank 0とbootloader settingsを変更していない。

## M4: LTE OTA E2E

更新後もLTE OTA機能を保持するため、単純な`blinky_v2`ではなく、`lte_bootstrap`を基にした`lte_target_v3`を更新対象とした。

- Version: 3
- Size: 103,988 bytes
- CRC16-CCITT: `0x235b`
- SHA-256: `b6c56f8dee6fd86aa10837c752a725fac722938770def1e1a8bd592c669ac950`
- Harvest Files path: `/wio-bg770a/v3/firmware.bin`

Harvest Filesへのアップロード後に再取得したSHA-256が一致することを確認した。Metadataはenabledかつread-onlyを維持したまま、ユーザーデータだけをversion 3のmanifestへ更新し、読み戻した値が配信イメージと一致することを確認した。

自動適用を有効にした`lte_bootstrap_apply`をUSB DFUで書き込んだ後、次のログを実機で確認した。

```text
[HTTP] manifest status=200 length=245
[OTA] downloading version=3 size=103988
[HTTP] firmware status=200 length=103988
[HTTP] UFS response stored
[HTTP] UFS file opened handle=0
[OTA] progress 16384/103988
[OTA] progress 32768/103988
[OTA] progress 49152/103988
[OTA] progress 65536/103988
[OTA] progress 81920/103988
[OTA] progress 98304/103988
[OTA] progress 103988/103988
[OTA] image verified
[OTA] activated; rebooting
```

USB再接続後、更新されたv3はLTEへ再接続して同じmanifestを取得し、次を出力した。

```text
[HTTP] manifest status=200 length=245
[OTA] no update current=3 manifest=3
[STATUS] phase=ota-check-complete sim=...<LAST4>
```

これにより、SORACOM MetadataとHarvest Filesだけを配信経路として、v1のOTA bootstrapからOTA機能を保持したv3へのダウンロード、完全性検証、Bank 1有効化、bootloader反映、更新後起動、更新不要判定までを確認し、M4の完了条件を満たした。

## WioOtaAgentライブラリ化後のv3→v4確認

M4で`main.cpp`に入っていたmanifest取得、技術的検証、Harvest Files download、Bank 1検証・適用処理を`WioOtaAgent` 0.1.0へ抽出した。更新判断はユーザーアプリの`DecisionCallback`へ移した。

更新対象v4:

- Version: 4
- Size: 131,784 bytes
- CRC16-CCITT: `0xbde9`
- SHA-256: `7c51d4f6976c844a05ad5dbc268dd0be05e42b601456838dca8743b7842d87ae`
- Harvest Files path: `/wio-bg770a/v4/firmware.bin`
- 自動適用: 有効

Harvest Filesへ配置したv4を再取得し、サイズとSHA-256がローカル成果物と一致することを確認した。Metadataはenabledかつread-onlyを維持したまま、userdataだけをversion 4へ変更し、サイズ、CRC16、SHA-256を読み戻し確認した。

既存のv3は自動適用が無効だったため、ライブラリ化したAgentと自動適用を有効にした`lte_target_v3_apply`をUSB DFUで試験起点として導入した。その後はLTEだけで次の処理が完了した。

```text
[HTTP] manifest status=200 length=245
[OTA] downloading version=4 size=131784
[HTTP] firmware status=200 length=131784
[HTTP] UFS response stored
[HTTP] UFS file opened handle=0
[OTA] progress 16384/131784
[OTA] progress 32768/131784
[OTA] progress 49152/131784
[OTA] progress 65536/131784
[OTA] progress 81920/131784
[OTA] progress 98304/131784
[OTA] progress 114688/131784
[OTA] progress 131072/131784
[OTA] progress 131784/131784
[OTA] image verified
[OTA] activated; rebooting
```

bootloader反映後のv4はLTEへ再接続し、同じmanifestに対してユーザーアプリの判断コールバックを実行した。

```text
[HTTP] manifest status=200 length=245
[OTA] no update current=4 manifest=4
[OTA] application reports no update
[STATUS] phase=ota-check-complete sim=...<LAST4>
```

これにより、ライブラリ化したAgentによるmanifest取得、アプリ判断、download、検証、適用、再起動と、更新後アプリにAgentが保持されていることを実機で確認した。v4では自動適用も有効なため、次バージョンへの継続OTAが可能な構成になっている。

## 次の作業

M5として、通信断、ダウンロード途中の電源断、破損イメージ、容量超過、bootloader settings更新前後の電源断を試験する。本番投入前にはM6として電子署名とアンチロールバックも実装する。
