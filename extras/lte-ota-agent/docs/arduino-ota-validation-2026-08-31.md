# Arduinoビルド同士のLTE OTA試験（2026-08-31）

## 試験範囲

Arduino CLIでビルド・USB導入済みのversion 4から、同じCLIでビルドしたversion 5へ
LTEだけで更新する。version 5のUSB書込みは行わない。
起点の実機確認は[Arduino CLI実機検証記録](arduino-hardware-validation-2026-08-31.md)を参照する。

- 実機: Wio BG770A HW v1.0、dual-bank bootloader、S140 7.3.0
- Arduino CLI 1.2.2、SeeedJP core 1.5.1
- WioCellular 0.3.15、ArduinoJson 7.0.4、WioOtaAgent 0.2.1
- 共通スケッチ: `examples/cellular-status-ota/CellularStatusOta/CellularStatusOta.ino`
- 公開鍵・key_idは起点のv4と同一。署名必須・アンチロールバック・段階配信を維持
- 配信対象グループのSIMがactiveな試験SIM 1台だけであることをAPIで確認
- Metadataはenabled/read-only、Harvest Filesはenabledを維持

## Arduino version 5の生成: PASS

既存Arduino環境を変更せず、プロジェクト内の隔離環境を使用した。

```bash
export ARDUINO_DIRECTORIES_DATA="$PWD/.pio/arduino-cli/data"
export ARDUINO_DIRECTORIES_DOWNLOADS="$PWD/.pio/arduino-cli/downloads"
export ARDUINO_DIRECTORIES_USER="$PWD/.pio/arduino-cli/user"
arduino-cli compile \
  --fqbn SeeedJP:nrf52:wio_bg770a:board_version=1_0,softdevice=s140v7,debug_output=serial \
  --build-property compiler.cpp.extra_flags=-DAPP_VERSION=5 \
  --build-path "$PWD/.pio/arduino-build-hardware-v5" \
  examples/cellular-status-ota/CellularStatusOta
```

Flash表示172,352 bytes、RAM 21,264 bytes。DFU ZIPはapplication-onlyであることを確認した。
生成済みの試験鍵を使用し、format 2 manifestとraw binaryを作った。

```bash
python3 tools/firmware_manifest.py \
  .pio/arduino-build-hardware-v5/CellularStatusOta.ino.zip \
  --version 5 \
  --url http://harvest-files.soracom.io/wio-bg770a/arduino-ota-test/20260831-v5/firmware.bin \
  --signing-key .pio/m6-test/signing-key.pem \
  --key-id m6-test-2026 \
  --release-id arduino-test-release-5-20260831 \
  --rollout 10000 \
  --output .pio/arduino-ota-v5/manifest-v5.json \
  --firmware-output .pio/arduino-ota-v5/firmware-v5.bin
```

鍵と生成物はGit管理対象外である。上記の鍵は既存実機と一致するローカル試験鍵であり、
第三者が再現する場合は、自分の端末へ組み込んだ鍵に置き換える。

```text
raw image size: 172360 bytes
CRC16: 6763
SHA-256: b40d28fe19d41c6269f5ee47ea5c3b5f96a27cb2116bb68ca053e86289f25dad
```

PC上で公開鍵による署名検証、CRC16/SHA-256、DFU ZIP内部のbinaryとの完全一致を確認した。
ビルド設定のAPP_VERSION=5とEd25519検証関数のリンクも確認した。

## 配信準備: PASS

1. Harvest Filesの既存一覧で新しい配信パスが未使用であることを確認した。
2. raw binaryをprivate scopeへアップロードした。
3. APIでダウンロードし、172,360 bytesのファイル全体とSHA-256が元の生成物に一致した。
4. 配信直前にグループ設定・所属SIMを再取得し、事前確認から変わっていないことを確認した。
5. `SoracomAir`の`userdata`だけを署名付きv5 manifestへ更新した。
6. APIの読み戻しでmanifestの完全一致と、userdata以外の設定が不変であることを確認した。

Metadata設定の更新形式は[SORACOM公式仕様](https://users.soracom.io/ja-jp/docs/air/use-metadata/)
の`key`・`value`配列を使用した。端末からの書込みを許可する設定変更はしていない。
旧v4 manifestと設定のスナップショットは`.pio/arduino-ota-v5/group-before.json`に保存した。

## 実機E2E: PASS

配信切替後も端末がversion 4で稼働していることを確認した。
このサンプルは起動時に1回だけOTAを確認するため、08:29頃に通常resetを行った。
USB再接続に追従するpyserialの収集処理で、更新前から更新後までログを取得した。
version 5へのUSB書込みは行っていない。

### v4による取得・検証・適用

```text
[2026-08-31T08:29:57] [LTE] network ready
[2026-08-31T08:29:57] [OTA] security state loaded=4 current=4 highest=4
[2026-08-31T08:29:58] [HTTP] manifest status=200 length=481
[2026-08-31T08:29:59] [OTA] downloading version=5 size=172360
[2026-08-31T08:30:06] [HTTP] firmware status=200 length=172360
[2026-08-31T08:30:11] [HTTP] UFS response stored
[2026-08-31T08:30:18] [OTA] progress 16384/172360
[2026-08-31T08:30:39] [OTA] progress 65536/172360
[2026-08-31T08:31:06] [OTA] progress 131072/172360
[2026-08-31T08:31:23] [OTA] progress 172360/172360
[2026-08-31T08:31:23] [OTA] image verified
[2026-08-31T08:31:24] [OTA] activated; rebooting
```

署名必須のv4がmanifest検証を通過し、Harvest Filesから172,360 bytesを取得した。
BG770AのUFSへ保存した応答をBank 1へ転送し、CRC16/SHA-256検証後にbootloaderへ
適用登録して再起動した。

### 自動再起動後のv5

```text
[2026-08-31T08:31:32] cellular-status + OTA, app-version=5
[2026-08-31T08:31:45] [LTE] network ready
[2026-08-31T08:31:45] [OTA] security state loaded=4 current=5 highest=5
[2026-08-31T08:31:47] [HTTP] manifest status=200 length=481
[2026-08-31T08:31:47] [OTA] no update: manifest version already installed
[2026-08-31T08:31:47] [OTA] result=no update
[2026-08-31T08:31:47] [APP] version=5 uptime=17
[2026-08-31T08:31:52] [APP] version=5 uptime=22
[2026-08-31T08:31:57] [APP] version=5 uptime=27
[2026-08-31T08:32:02] [APP] version=5 uptime=32
```

bootloaderによる反映後にv5が起動した。`loaded=4`は更新前の最高適用versionを
不揮発メモリから読み出した値で、`highest=5`は現在versionの記録・読み戻し検証後の値である。
同じ署名付きmanifestを再取得して更新なしと判定し、通常loopの動作が継続した。
開始から全量受信・検証まで約84秒、自動再起動から更新なし判定まで約23秒だった。

## 終了時の状態と検証範囲

- 端末はArduinoビルドのversion 5で稼働
- Metadataは正常な署名付きv5 manifest、enabled/read-onlyを維持
- 配信対象は引き続きactiveな試験SIM 1台
- Harvest Filesには署名対象と一致するv5 binaryを保持
- v5のUSB書込み、bootloader変更、PCの既存Arduino環境の変更はなし

APIの読み戻しで配信manifestの完全一致と署名、Metadata設定、所属SIMを確認した。
また、ログ中の取得・検証・適用・v5起動・更新なし・loop継続がこの順番で現れることを
機械的に照合した。生成物・設定スナップショット・ログは`.pio/arduino-ota-v5`に保存し、
公開する記録には秘密鍵やSIM固有情報を含めていない。

これにより、PlatformIO版に加えてArduino CLIでビルドした`.ino`でも、LTE OTAの
一連の更新を実機で確認できた。この試験にはGUI操作を含めていない。
後続の[Arduino IDE GUI検証記録](arduino-ide-gui-validation-2026-08-31.md)で、
GUIのコンパイル・USB書込み・更新なし判定と`loaded=5`の読み戻しを確認した。
ArduinoビルドでのM5障害注入・M6拒否条件の全再試験と、v5をさらに通常resetした後の
`loaded=5`の独立した再読出し確認は、今回のE2E試験には含めていない。
