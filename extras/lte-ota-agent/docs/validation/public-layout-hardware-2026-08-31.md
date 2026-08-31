# 構成整理後のLTE OTA実機検証（2026-08-31）

HW v1.0の1台で、既存アプリから整理後のPlatformIOビルドへ、続けてArduino CLIビルドへLTE OTAしました。
両方でイメージ検証、適用登録、更新後アプリの起動、同じmanifestに対する「更新なし」判定、通常ループへの復帰を確認しました。

対象ソースは[構成整理コミット](https://github.com/takao2704/wio_cellular/tree/aac6f8a1598118efbaf7b1ea5ce35d1568b7c460/extras/lte-ota-agent)です。
[PC上の検証](public-layout-2026-08-31.md)に続く実機試験で、途中にUSB書込みは挟んでいません。
bootloader、SoftDevice、署名鍵、保存済みバージョンは試験前のものを引き継ぎました。

## 条件と配信イメージ

- SORACOM MetadataとHarvest Filesを使用。接続先は共通スケッチの`metadata.soracom.io`と`harvest-files.soracom.io`
- Metadataは有効・read-only、Harvest Filesは有効。配信先グループの所属SIMは対象の1枚のみ
- 署名必須、アンチロールバック、配信対象の判定を有効化。署名付きmanifestの配信率は100%
- 各バイナリを別の未使用パスへアップロードし、ダウンロードし直して元データとの一致を確認。その後にMetadataのユーザーデータだけを変更
- 通常のRESETでサンプルの起動時チェックを開始。更新中のリセット・電源断は行わない

| ビルド環境 | 配信するraw binaryのサイズ | SHA-256 |
|---|---:|---|
| PlatformIO | 172,900 bytes | `7abf0fcb3fcc9bd28e000ec6b402e853943da19067b8dc9daccf0dbe206a1386` |
| Arduino CLI | 172,360 bytes | `c41464d4484e3743fdbe681d1dd1e3d6926a77bbb044640bcaa9f5a9b79afcdb` |

ログ中の5・6・7は今回の更新順序を識別するアプリの値で、ライブラリのリリース番号ではありません。
既存のバージョン記録を消さずに試験するため、適用済みの値より大きい値を使いました。

PlatformIO 6.1.18とArduino CLI 1.2.2を使用しました。
Arduino CLIはSeeedJP nRF52 Boards 1.5.1、WioCellular 0.3.15、ArduinoJson 7.0.4の環境です。
共通スケッチと、このツリーの`WioOta`・`WioBg770aHttp`・`WioOtaAgent`をビルド対象にしました。
公開鍵と署名用の秘密鍵、配信先の実識別子、生成物はGitへ追加していません。

## ビルド方法

PlatformIOは[署名必須の設定](../platformio.md)と同じ`env.build_flags`に
`-DAPP_VERSION=6`と`-DWIO_OTA_SECURE`を加えたローカル環境でビルドしました。
Arduino CLIは既定で署名必須の共通スケッチに`-DAPP_VERSION=7`を指定しました。
ビルド引数の構成は次のとおりです。作業ディレクトリとCLIのデータ保存先は省略しています。

```bash
arduino-cli compile \
  --fqbn SeeedJP:nrf52:wio_bg770a:board_version=1_0,softdevice=s140v7,debug_output=serial \
  --build-property compiler.cpp.extra_flags=-DAPP_VERSION=7 \
  --library lib/WioOta \
  --library lib/WioBg770aHttp \
  --library lib/WioOtaAgent \
  --build-path .pio/arduino-target \
  examples/cellular-status-ota/CellularStatusOta
```

各application DFU ZIPから`tools/firmware_manifest.py`でraw binaryと署名付きmanifestを生成しました。
アプリとmanifestのバージョンを一致させ、既存アプリが信頼している鍵で署名しています。
鍵の準備と配信手順は[署名と段階配信](../security-and-rollout.md)を参照してください。
再試験時は、実機の適用済みバージョンより大きい値と新しい配信パスを使います。

## PlatformIOビルドへの更新

ホストで付けた時刻はJSTです。シリアルログからOTAに関係する行を抜粋しています。

```text
[2026-08-31T11:58:51] [OTA] security state loaded=5 current=5 highest=5
[2026-08-31T11:58:53] [HTTP] manifest status=200 length=485
[2026-08-31T11:58:53] [OTA] downloading version=6 size=172900
[2026-08-31T11:59:00] [HTTP] firmware status=200 length=172900
[2026-08-31T12:00:17] [OTA] progress 172900/172900
[2026-08-31T12:00:18] [OTA] image verified
[2026-08-31T12:00:18] [OTA] activated; rebooting
[2026-08-31T12:00:25] cellular-status + OTA, app-version=6
[2026-08-31T12:00:39] [LTE] network ready
[2026-08-31T12:00:39] [OTA] security state loaded=5 current=6 highest=6
[2026-08-31T12:00:41] [HTTP] manifest status=200 length=485
[2026-08-31T12:00:41] [OTA] no update: manifest version already installed
[2026-08-31T12:00:41] [OTA] result=no update
[2026-08-31T12:00:41] [APP] version=6 uptime=17
```

## Arduino CLIビルドへの更新

2回目はログ収集の時間制限が切れた後にRESETを押したため、収集を途中から再開しました。
この更新の開始時のmanifest取得・firmware HTTP応答ログは保存できていません。
UFSからの読み出し、全データの受信、検証、適用、更新後の起動とmanifest再取得は保存できています。

```text
[2026-08-31T14:23:00] [HTTP] UFS response stored
[2026-08-31T14:23:07] [OTA] progress 16384/172360
[2026-08-31T14:24:11] [OTA] progress 172360/172360
[2026-08-31T14:24:11] [OTA] image verified
[2026-08-31T14:24:12] [OTA] activated; rebooting
[2026-08-31T14:24:20] cellular-status + OTA, app-version=7
[2026-08-31T14:24:37] [LTE] network ready
[2026-08-31T14:24:37] [OTA] security state loaded=6 current=7 highest=7
[2026-08-31T14:24:39] [HTTP] manifest status=200 length=493
[2026-08-31T14:24:40] [OTA] no update: manifest version already installed
[2026-08-31T14:24:40] [OTA] result=no update
[2026-08-31T14:24:45] [APP] version=7 uptime=22
```

`loaded=6`から、前のアプリが保存したバージョンを今回のOTA後も読み出せたことを確認しました。
`highest=7`は今回の起動中の記録成功を表します。

## 最後のRESET後の読み戻し

更新完了後に通常のRESETをもう一度押し、保存済みバージョンの読み戻しと通常動作への復帰を確認しました。

```text
[2026-08-31T14:28:35] [LTE] network ready
[2026-08-31T14:28:35] [OTA] security state loaded=7 current=7 highest=7
[2026-08-31T14:28:37] [HTTP] manifest status=200 length=493
[2026-08-31T14:28:38] [OTA] no update: manifest version already installed
[2026-08-31T14:28:38] [OTA] result=no update
[2026-08-31T14:28:43] [APP] version=7 uptime=23
```

`loaded=7`は新しいアプリが前回の起動中に保存した値です。
同じmanifestからの再ダウンロード・再適用は発生せず、アプリのループへ戻りました。

## 検証範囲

今回確認したのは、整理後のPlatformIO版とArduino CLI版を順に適用する正常系のOTAです。
Arduino IDE GUIで新たにビルドしたアプリ間のOTA、HW v1.1、多台数での段階配信、
障害注入・電源断試験は実施していません。過去の結果は[検証一覧](README.md)で区別しています。
