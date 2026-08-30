# Wio BG770A cellular OTA

Wio BG770A HW v1.0を、USBで一度だけOTA対応基盤へ移行し、その後LTEでユーザーアプリを更新するための開発リポジトリです。

M0〜M4は実機で完了しています。dual-bank bootloaderへの移行、bootstrap書込み、Bank 1へのローカル転送と反映に加え、SORACOM MetadataとHarvest Filesを使ったLTE OTAを確認済みです。ライブラリ化後の`WioOtaAgent`でも131,784-byteのv4を取得し、CRC16/SHA-256検証、Bank 1有効化、bootloader反映、v4起動後の更新不要判定まで成功しています。

## ビルド

```bash
pio run
```

- `bootstrap`: USB serialからraw firmwareを受け、Bank 1へ保存する初期アプリ
- `blinky_v2`: 更新成功確認用の対象アプリ
- `lte_bootstrap`: manifestとraw firmwareをLTE HTTPで取得する初期アプリ（既定では無効）
- `lte_bootstrap_apply`: M4検証用。LTE OTAを有効化し、検証済みイメージを自動適用する初期アプリ
- `lte_target_v3`: 更新後もLTE OTA機能を保持するversion 3アプリ
- `lte_target_v3_apply`: ライブラリ化後のv3→v4実機試験用起点
- `lte_target_v4`: `WioOtaAgent`を内蔵し、将来のOTA自動適用も保持するversion 4アプリ

## ライブラリ

- `WioOta`: 転送元に依存しないdual-bank writer
- `WioBg770aHttp`: BG770A内蔵HTTPクライアントとUFS読出し
- `WioOtaAgent`: manifest取得、ユーザーアプリの更新判断、download/verify/applyの制御

SORACOM Metadataのユーザーデータをmanifest、SORACOM Harvest Filesをfirmware本体の配信元として使います。BG770A内蔵HTTPクライアントで取得し、大きなレスポンスはモデム内UFSへ一時保存して512-byte単位でBank 1へ転送します。M4では更新後のv3が同じmanifestを再取得し、`current=3 manifest=3`として更新不要と判定できることも確認しています。

既存ユーザーアプリへの追加方法と1日1回の確認例は[WioOtaAgent組み込みガイド](docs/ota-library-integration.md)、開発状況は[開発計画](docs/development-plan.md)を参照してください。

公式WioCellularの`cellular-status` exampleを起点にした最小構成は
[`examples/cellular-status-ota`](examples/cellular-status-ota)にあります。
`ota_v1`を書き込んだ後、`ota_v2`をHarvest Filesから配信することで、
既存アプリへAgentを追加して更新する流れを確認できます。

## 自動テスト

```bash
tools/run_native_tests.sh
```

CRC16、SHA-256、manifestの拒否条件に加え、ファームウェア受信の完了、
途中切断、CRC16不一致、SHA-256不一致をPC上で検証します。実機での通信断と
電源断は[M5障害注入試験](docs/m5-fault-injection.md)に従って段階的に実施します。

署名付きmanifest、アンチロールバック、段階配信は
[M6セキュリティ手順](docs/m6-security-and-rollout.md)を参照してください。実装・自動試験に加え、
HW v1.0実機で署名付き更新、改ざんと旧versionの拒否、配信率0%での延期と100%での更新、
再起動後のversion保存状態を確認しました。[実機検証記録と検証範囲](docs/m6-hardware-validation-2026-08-31.md)を参照してください。
