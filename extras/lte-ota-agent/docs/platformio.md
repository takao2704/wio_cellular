# PlatformIOでCellularStatusOtaを使う

[Arduino IDE版](arduino-ide.md)と同じスケッチをビルドします。
この文書のコマンドはOTAプロジェクトのルート
（wio_cellular内では`extras/lte-ota-agent`）で実行します。

## 前提

[bootloaderの確認・移行](bootloader-migration.md)を済ませたHW v1.0を使います。
サンプルの設定にはSeeedJP nRF52プラットフォームとCoreのコミット、
WioCellular 0.3.15、ArduinoJson 7.0.4を固定しています。
既存プロジェクトへ追加する場合は[組み込みガイド](ota-library-integration.md)を参照してください。

`initial`と`update`は同じスケッチで、アプリの識別値だけが1と2です。
ライブラリの版番号ではありません。運用中の端末では最高適用バージョンを超える値に変更し、
manifestのversionも一致させます。

## 署名を使う設定

標準の2環境は更新処理を確認するための署名なしデモです。本番配布には使わないでください。
署名付きOTAでは、[鍵の準備](security-and-rollout.md#鍵を用意する)に従い、公開鍵ヘッダを
`examples/cellular-status-ota/src/ota_manifest_public_key.h`へ生成します。
秘密鍵はプロジェクト外の署名環境で管理します。

`examples/cellular-status-ota/platformio.local.ini`を作ります。このファイルはGit管理対象外です。

```ini
[platformio]
extra_configs = platformio.ini
default_envs = signed_initial, signed_update

[env:signed_initial]
extends = env:initial
build_flags =
    ${env:initial.build_flags}
    -DWIO_OTA_SECURE

[env:signed_update]
extends = env:update
build_flags =
    ${env:update.build_flags}
    -DWIO_OTA_SECURE
```

```bash
(cd examples/cellular-status-ota && pio run -c platformio.local.ini -e signed_initial)
(cd examples/cellular-status-ota && pio run -c platformio.local.ini -e signed_update)
```

`WIO_OTA_SECURE`により、署名必須、アンチロールバック、段階配信とバージョン保存が有効になります。
公開鍵ヘッダがなければコンパイルは失敗します。

## 初回USB導入と配信

USB書込みは現在のアプリを置き換えます。対象SIMグループのMetadataを確認し、
意図しない更新が公開されていない状態で実施してください。

```bash
(cd examples/cellular-status-ota && pio run -c platformio.local.ini -e signed_initial -t upload)
```

自動でDFUへ移行しない場合だけRESETをダブルクリックして再実行します。
次回用のアプリはUSB書込みせず、生成されたapplication用ZIPを配信ツールへ渡します。

```bash
python3 tools/firmware_manifest.py \
  examples/cellular-status-ota/.pio/build/signed_update/firmware.zip \
  --version 2 \
  --url http://harvest-files.soracom.io/wio-bg770a/release-2/firmware.bin \
  --signing-key /path/to/signing-key.pem \
  --key-id production-2026 \
  --release-id release-2 \
  --rollout 10000 \
  --output dist/release-2/manifest.json \
  --firmware-output dist/release-2/firmware.bin
```

署名ツールにはPythonの`cryptography`が必要です。
`--key-id`は端末へ組み込んだ公開鍵のIDに合わせます。
先にraw binaryをHarvest Filesへ配置して内容を確認し、
対象グループのMetadataを対応するmanifestへ切り替えます。
Metadataは有効・read-only、Harvest Filesは有効にします。
端末は次にOTA確認を実行したときに取得します。サンプルでは通常リセット後の起動時です。

LTE接続、ダウンロード、検証、再起動、更新後のアプリ識別値と「更新なし」判定を確認します。
サービス利用料金が発生するため、試験対象と配信先を確認してから操作してください。
FQDNは`metadata.soracom.io`と`harvest-files.soracom.io`、ポートは80です。

## 署名なしの機能確認と保守試験

署名なしデモだけをビルドする場合は、通常の設定を使います。

```bash
pio run -d examples/cellular-status-ota -e initial
pio run -d examples/cellular-status-ota -e update
```

停止・リセットの試験環境は[実機試験](fault-injection.md)へ分離しています。
通常のexampleをビルドしても、停止用のフラグは入りません。
