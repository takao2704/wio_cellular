# Arduino CLI実機確認（2026-08-31）

## 対象と変更範囲

- Wio BG770A HW v1.0。開始前に通常USBモードとversion 4の稼働ログを確認
- 共通スケッチ: `examples/cellular-status-ota/CellularStatusOta/CellularStatusOta.ino`
- Arduino CLI 1.2.2、SeeedJP core 1.5.1、WioCellular 0.3.15、ArduinoJson 7.0.4
- ZIPから導入したWioOta / WioBg770aHttp / WioOtaAgentを使用
- 公開鍵とAPP_VERSION=4を維持。署名必須、アンチロールバック、段階配信を有効化
- bootloader、SORACOMの設定、PCの既存Arduino環境は変更しない

事前にAPIで、配信manifestが保存済みの正常な署名付きv4と完全一致すること、
公開鍵による署名検証、Metadataのenabled/read-only、Harvest Filesのenabledを確認した。
今回は同じversionでのUSB導入と起動確認が対象であり、新versionへのLTE OTA配信試験ではない。

## ビルド: PASS

プロジェクト内の隔離したArduino CLI環境を使用した。

```bash
export ARDUINO_DIRECTORIES_DATA="$PWD/.pio/arduino-cli/data"
export ARDUINO_DIRECTORIES_DOWNLOADS="$PWD/.pio/arduino-cli/downloads"
export ARDUINO_DIRECTORIES_USER="$PWD/.pio/arduino-cli/user"
arduino-cli compile \
  --fqbn SeeedJP:nrf52:wio_bg770a:board_version=1_0,softdevice=s140v7,debug_output=serial \
  --build-property compiler.cpp.extra_flags=-DAPP_VERSION=4 \
  --build-path "$PWD/.pio/arduino-build-hardware-v4" \
  examples/cellular-status-ota/CellularStatusOta
```

- Flash表示: 172,352 bytes / 397,312 bytes
- RAM表示: 21,264 bytes
- application-only DFU ZIP、raw binary 172,360 bytes
- raw binary SHA-256: `f01d69edc2cc28ceca38d75f6eb68526b32163764ede90631fb1c9aa4a094ae3`
- mapで`CRYS_ECEDW_Verify`のリンクを確認
- Core由来の`used attribute ignored`警告あり。ビルドは終了コード0

## 自動DFUからのUSB書込み: 未成功

通常モードのポートを指定し、Arduino CLIの`upload --build-path`を実行したが、
転送ツールが次のエラーを出力した。

```text
Timed out waiting for acknowledgement from device.
Failed to upgrade target. Error is: Attempting to use a port that is not open
```

この場合もArduino CLIの終了コードは0だった。終了コードのみを成功条件にしない。
この試行では書込み後の起動ログも得られず、実機動作の確認には使えなかった。

失敗後にUSB VID:PID `2886:8056`（通常モード）と、シリアルでversion 4の稼働を確認した。
開始前からuptimeが変わっているため、試行中に再起動が発生している。

```text
[2026-08-31 07:59:49] [APP] version=4 uptime=51
[2026-08-31 07:59:54] [APP] version=4 uptime=56
```

## 手動DFUからのUSB書込み: PASS

RESETのダブルクリック後、DFUモードのVID:PID `2886:0056`を確認した。
シリアルモニタを閉じ、同じ生成済みZIPをArduino CLIで再送した。
手動でDFUへ入っているため、自動1200 bps touchとポート待ちを無効化した。

```bash
arduino-cli upload \
  --fqbn SeeedJP:nrf52:wio_bg770a:board_version=1_0,softdevice=s140v7,debug_output=serial \
  --port /dev/cu.usbmodem101 \
  --upload-property upload.use_1200bps_touch=false \
  --upload-property upload.wait_for_upload_port=false \
  --build-path "$PWD/.pio/arduino-build-hardware-v4" \
  examples/cellular-status-ota/CellularStatusOta
```

```text
Activating new firmware
Device programmed.
```

終了コード0に加えて転送ツールの完了出力を確認し、Arduino CLIのシリアルモニタで
新たなuptimeのversion 4稼働ログを確認した。

```text
[2026-08-31 08:04:04] [APP] version=4 uptime=29
[2026-08-31 08:04:09] [APP] version=4 uptime=34
[2026-08-31 08:04:14] [APP] version=4 uptime=39
```

USB書込みと通常loopの動作は確認できた。モニタの接続前にsetupのログ出力が終わったため、
LTE接続・署名付きmanifest判定は、通常reset後のログを別途取得した。

## 通常reset後のLTE接続・OTA判定: PASS

USBの切断・再接続に追従するpyserialの収集処理を起動し、RESETを1回押した。
ビルド・書込みはArduino CLIで行い、この再起動試験のログ収集だけpyserialを使用した。

実測ログの抜粋（端末固有情報を含むAT通信ログは掲載しない）:

```text
[2026-08-31T08:06:39] [LTE] network ready
[2026-08-31T08:06:39] [OTA] security state loaded=4 current=4 highest=4
[2026-08-31T08:06:39] [HTTP] modem HTTP client configured
[2026-08-31T08:06:39] [HTTP] GET metadata manifest
[2026-08-31T08:06:41] [HTTP] manifest status=200 length=449
[2026-08-31T08:06:41] [OTA] no update: manifest version already installed
[2026-08-31T08:06:41] [OTA] result=no update
[2026-08-31T08:06:41] [APP] version=4 uptime=15
[2026-08-31T08:06:46] [APP] version=4 uptime=20
[2026-08-31T08:06:51] [APP] version=4 uptime=25
[2026-08-31T08:06:56] [APP] version=4 uptime=30
```

- LTE接続後、FQDN指定のMetadataからHTTP 200でmanifestを取得した。
- `loaded=4`は、現在versionを記録する前に不揮発メモリから読んだ値である。
  USB書込み・通常resetをまたいで最高適用version 4が保持されている。
- 署名必須設定では`evaluateManifestSecurity()`がEd25519署名を検証してから同一versionを
  判定する。このログの`no update: manifest version already installed`は、署名検証を
  通過した後の判定である。
- 更新なしの結果を返した後も、5秒周期の通常loopが継続した。

reset直後には`WARNING: Interface is active when begin()`も出力されたが、上記のLTE接続・
manifest取得・判定は完了した。この警告を解消する変更は今回行っていない。

## 結論と残る範囲

Arduino CLIで共通`.ino`をビルドし、HW v1.0へUSB書込みして、通常loop、LTE接続、
署名付きmanifestの同一version判定、最高適用versionの保持まで確認した。
このUSB導入試験の終了時にはArduinoビルドのversion 4で動作していた。
この試験ではbootloader、SORACOMの配信設定、PCの既存Arduino環境は変更していない。

この試験はUSB導入と動作確認までを扱う。後続のArduinoビルド同士のv4→v5 LTE OTAは
[別の検証記録](arduino-ota-validation-2026-08-31.md)で成功を確認した。
この試験にはGUI操作を含めていない。後続の[Arduino IDE GUI検証記録](arduino-ide-gui-validation-2026-08-31.md)
で、GUIのコンパイル・USB書込みとシリアルモニタでの実機動作を確認した。
