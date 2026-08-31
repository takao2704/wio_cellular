# WioOtaAgent 組み込みガイド

## ライブラリの分担

既存のWio BG770Aユーザーアプリへ、次の3ライブラリを追加する。

| ライブラリ | 責務 |
|---|---|
| `WioOta` | Bank 1の消去・書込み・CRC16/SHA-256検証・bootloader登録 |
| `WioBg770aHttp` | BG770A内蔵HTTPクライアントとUFS経由の512-byte読出し |
| `WioOtaAgent` | manifest取得・技術的検証・アプリ判断の呼出し・download/verify/applyの制御 |

`WioOtaAgent`はLTE attachやPSM制御を行わない。ユーザーアプリがPDP contextを利用可能な状態にした後で`Agent::check()`を呼ぶ。更新なし、延期、検証のみの場合のモデム停止やPSM復帰もユーザーアプリが担当する。更新適用時だけ、AgentがBG770Aを`powerOff()`してbootloader settingsを更新し、nRF52840を再起動する。

## 導入条件

- HW v1.0向けdual-bank bootloaderとS140 7.3.0が導入済み
- `BOARD_VERSION_1_0`でビルド
- `board_upload.maximum_size = 397312`を設定
- `WioCellular` 0.3.15とArduinoJson 7.0.4を使用
- OTA開始前にSoftDeviceを無効化できる
- 更新対象はapplication raw binaryで、397,312 bytes以下

Arduino IDEで使う場合は[Arduino IDEガイド](arduino-ide.md)を参照する。
3ライブラリのZIPと共通の`.ino`を用意しており、下記のPlatformIO設定は不要である。
SeeedJP core 1.5.1のArduino IDEボード定義には397,312 bytesの上限が設定済みである。

既存PlatformIOプロジェクトへ`lib/WioOta`、`lib/WioBg770aHttp`、`lib/WioOtaAgent`をコピーする。最小のビルド設定は次のとおり。

```ini
[env:wio_bg770a]
board_upload.maximum_size = 397312
build_flags =
    -DBOARD_VERSION_1_0
    -DCFG_LOGGER=0
lib_archive = no
lib_deps =
    seeedjp/WioCellular@0.3.15
    bblanchon/ArduinoJson@7.0.4
```

署名検証を使うPlatformIO環境では、`WioOtaAgent`の`library.json`に含まれるbuild
scriptがArduino Core内のCryptoCellライブラリを解決する。Arduino IDEではCoreに
同梱された`Adafruit_nRFCrypto`をそのまま利用し、nRF52840向け署名検証実装を自動的に
コンパイルする。PlatformIOの独自アプリではbuild flagに`WIO_OTA_ENABLE_ED25519`を
追加する。PlatformIOのexampleは`WIO_OTA_SECURE`で有効化する。
いずれも署名を必須にするには`config.security.require_signature = true`を設定する。

ボードパッケージとビルド設定の完全例は[PlatformIOガイド](platformio.md)、
実機側の確認・移行は[bootloader移行ガイド](bootloader-migration.md)を参照する。

## 1回分のOTA確認

以下は既存アプリへ追加する部分の例であり、単独のスケッチではない。
`batteryVoltageTooLow()`や`userApplicationIsBusy()`などの業務関数は利用側で実装する。
ビルド可能な共通スケッチは[CellularStatusOta](../examples/cellular-status-ota/CellularStatusOta/CellularStatusOta.ino)を参照する。

Configにはmanifest取得先と、firmware URLに許可するhost/portを指定する。manifestに別hostが書かれていてもAgentが拒否する。

```cpp
#include <WioCellular.h>
#include <WioOtaAgent.h>

constexpr uint32_t kApplicationVersion = 1;

wio_ota_agent::Decision decideUpdate(
    const wio_ota_agent::Manifest& manifest) {
  if (manifest.version <= kApplicationVersion) {
    return wio_ota_agent::Decision::kNoUpdate;
  }
  if (batteryVoltageTooLow() || userApplicationIsBusy()) {
    return wio_ota_agent::Decision::kDefer;
  }
  return wio_ota_agent::Decision::kApply;
}

void reportProgress(size_t received, size_t total) {
  Serial.printf("OTA %u/%u\n", static_cast<unsigned>(received),
                static_cast<unsigned>(total));
}

wio_ota_agent::Result checkOtaOnce() {
  wio_ota_agent::Config config;
  config.target_hardware = "wio-bg770a-v1.0";
  config.manifest_host = "metadata.soracom.io";
  config.manifest_port = 80;
  config.manifest_path = "/v1/userdata";
  config.allowed_firmware_host = "harvest-files.soracom.io";
  config.allowed_firmware_port = 80;
  config.pdp_context_id = WioNetwork.config.pdpContextId;

  // Manifestを保持するため、ローカルスタックではなく静的領域へ置く。
  static wio_ota_agent::Agent agent{WioCellular, config, &Serial};
  return agent.check(decideUpdate, reportProgress);
}
```

`Decision`の意味は次のとおり。

| 値 | 動作 |
|---|---|
| `kReject` | 対象外としてdownloadしない |
| `kNoUpdate` | 現在のアプリと同一または古いためdownloadしない |
| `kDefer` | 更新はあるが端末状態により延期する |
| `kDownloadAndVerify` | Bank 1へ保存・検証した後、bootloaderへ登録せず破棄する |
| `kApply` | 保存・検証後にBG770Aを停止し、Bank 1を登録して再起動する |

更新判定はユーザーアプリのコールバックが担当する。`WIO_OTA_CURRENT_VERSION`のような特定のビルドフラグはAgentの必須仕様ではない。アプリ内定数、リリースID、永続化した適用済みIDなど、アプリが採用した識別方式を利用できる。

## 1日1回確認する例

`Agent::check()`は1回の確認を同期実行する。日次スケジュールはユーザーアプリのloopで管理する。次の例は起動時に確認し、その後24時間ごと、失敗または延期時は1時間後に再試行する。
`runUserApplication()`、`pauseUserApplicationForOta()`、`prepareCellularForOta()`、
`restoreNormalCellularPolicy()`、`resumeUserApplicationAfterOta()`は利用側で用意する。

```cpp
constexpr uint32_t kDailyIntervalMs = 24UL * 60UL * 60UL * 1000UL;
constexpr uint32_t kRetryIntervalMs = 60UL * 60UL * 1000UL;

uint32_t next_ota_check_at = 0;

bool deadlineReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

void loop() {
  runUserApplication();
  WioCellular.doWorkUntil(100);

  const uint32_t now = millis();
  if (!deadlineReached(now, next_ota_check_at)) {
    return;
  }

  pauseUserApplicationForOta();
  if (!prepareCellularForOta()) {
    next_ota_check_at = now + kRetryIntervalMs;
    resumeUserApplicationAfterOta();
    return;
  }

  const wio_ota_agent::Result result = checkOtaOnce();
  const bool retry = result == wio_ota_agent::Result::kFailed ||
                     result == wio_ota_agent::Result::kDeferred;
  next_ota_check_at = millis() +
                      (retry ? kRetryIntervalMs : kDailyIntervalMs);

  // kApplyの場合はcheckOtaOnce()内で再起動するため、ここへ戻らない。
  restoreNormalCellularPolicy();
  resumeUserApplicationAfterOta();
}
```

`millis()`方式は連続稼働中の24時間間隔であり、再起動すると起動時確認へ戻る。毎日決まった時刻に実行する場合は、ネットワーク時刻と永続化した最終確認日をユーザーアプリ側で利用する。

## manifestの技術的検証

アプリの判断コールバックを呼ぶ前に、Agentが次を検証する。

- JSONとして解析可能
- `format`が1または2
- `hardware`がConfigと一致
- image sizeが8〜397,312 bytes
- CRC16が4桁hexかつゼロではない
- SHA-256が64桁hex
- URLが`http://`形式
- firmware host/portがConfigの許可値と一致

format 2では、上記に加えてEd25519署名、`key_id`、anti-rollback floor、段階配信を
アプリの判断コールバックより先に検証できる。設定例、署名ツール、VersionStoreの
扱いは[署名・アンチロールバック・段階配信](security-and-rollout.md)を参照する。
CRC16とSHA-256だけのformat 1は破損検出用であり、本番の配信者認証には使わない。

## エラー取得

`Result::kFailed`の場合は、Agentから層別のエラーを取得できる。

```cpp
Serial.println(wio_ota_agent::errorString(agent.lastError()));
Serial.println(wio_bg770a_http::errorString(agent.lastHttpError()));
Serial.println(wio_ota::errorString(agent.lastWriterError()));
Serial.println(wio_ota_agent::securityErrorString(
    agent.lastSecurityError()));
```

`Agent::check()`はblockingかつ非再入APIであり、同時に複数のOTA確認を実行しない。ファームウェア取得中はユーザーアプリの時間制約がある処理を停止または退避する。途中失敗時はBank 0を変更せず、次回確認でBank 1を消去して最初から取得する。途中再開は未対応。
