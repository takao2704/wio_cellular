# Arduino IDE GUI試験（2026-08-31）

この文書は実施当時の記録です。旧環境名・コマンド・実測ログを保持しています。
現行の導入手順と検証範囲は[検証一覧](README.md)から参照してください。

## 現在の結果

- GUIの「検証」からのコンパイル: 成功
- GUIの「書き込み」: 自動DFU移行は失敗。RESETダブルクリック後の再試行で成功
- 今回GUIで生成したアプリの動作・LTE接続・更新なし判定: 成功
- GUIの「コンパイル済みバイナリをエクスポート」: 成功

Arduino CLIでの[v4→v5 LTE OTA試験](arduino-ota-2026-08-31.md)とは
別の試験である。今回の書込み前には、IDEのシリアルモニタで既存version 5の
`[APP] version=5`ログを確認した。これは今回生成したバイナリの動作証拠ではない。

## 環境と準備

| 項目 | 設定 |
|---|---|
| Arduino IDE | 2.3.10 / macOS |
| ボード | Seeed Wio BG770A、HW v1.0 |
| SeeedJP nRF52 Boards | 1.5.1 |
| SoftDevice / Print Port | S140 7.3.0 / Serial (USB-CDC) |
| WioCellular | 0.3.15 |
| ArduinoJson | 7.4.3（既存のまま。CLI試験の7.0.4とは異なる） |
| OTAライブラリ | WioOta 0.1.0、WioBg770aHttp 0.1.0、WioOtaAgent 0.2.1 |
| APP_VERSION / 署名検証 | 5 / 有効 |
| シリアルモニタ | 115200 bps |

実際のIDE環境にあるSeeedJP coreを1.0.1から1.5.1へ、WioCellularを0.1.4から
0.3.15へ更新し、OTA用の3ライブラリを導入した。依存パッケージの導入はIDEが使う
設定ファイルを指定したArduino CLIで行ったため、GUIの導入操作自体は未検証である。
IDE再起動後、ボードマネージャの1.5.1表示とシリアル接続を確認した。
旧WioCellularは`.pio/arduino-gui-environment-backup/WioCellular-0.1.4`へ保存した。
無関係なライブラリやボードパッケージは更新していない。

共通スケッチを`.pio/arduino-ide-v5/CellularStatusOta`へコピーしてIDEで開いた。
`.ino`本体と公開鍵は元ファイルとの完全一致を確認し、コピーの設定だけをversion 5にした。
既存の空スケッチ、bootloader、SORACOM配信設定、署名鍵は変更していない。
Metadataの既存署名付きv5 manifestが前のOTA試験の終了時と同一であることを読み取りで確認した。

## GUIコンパイル: PASS

IDEの「検証」を押し、次の出力とapplication-only DFU ZIPの生成を確認した。
GUIの書込み時に行われた再コンパイルも同じサイズだった。

```text
最大397312バイトのフラッシュメモリのうち、スケッチが175448バイト（44%）を使っています。
最大237568バイトのRAMのうち、グローバル変数が21264バイト（8%）を使っていて、ローカル変数で216304バイト使うことができます。
```

GUIのビルド設定ファイルで次のFQBNを確認した。

```text
SeeedJP:nrf52:wio_bg770a:board_version=1_0,softdevice=s140v7,debug_output=serial
```

生成ZIP内のapplication binaryは175,456 bytes。SHA-256は次の値だった。
ArduinoJsonのversionが違うため、先のCLI試験のバイナリとの同一性は主張しない。

```text
9e9ec01608f809cf930b388a86268befba1e7266b02d75841c302611127514cd
```

## GUI書込み: 手動DFUで成功

通常モードでIDEの「書き込み」を実行したが、出力末尾に次の診断が表示された。

```text
- Baud rate must be 115200, Flow control must be off.
- Target is not in DFU mode. Ground DFU pin and RESET and release both to enter DFU mode.
```

この最初の試行はUSB書込み成功とは扱わない。その後、ユーザーがRESETを素早く2回押し、
`BOOT`ボリュームのモデル表示とS140 7.3.0、シリアルポートを確認した。
IDEの「書き込み」を再度押すとDFU転送が進み、出力に次の行と「書き込み完了」が表示された。

```text
Activating new firmware
Device programmed.
```

転送されたDFU ZIP内のbinaryは、前節に記載したSHA-256と一致した。
CLIからの書込みやUF2ファイルのコピーには切り替えていない。

## GUIシリアルモニタ: PASS

IDEのシリアルモニタは115200 bpsで自動再接続した。表示位置が古いログに留まっていたため、
最新行へスクロールして確認した。起動冒頭の`app-version=5`行は取得できなかったが、
書込み完了後のLTE接続、セキュリティ状態、manifest取得、更新なし判定、
起動後20秒のversion 5のログを確認できた。

```text
09:57:27.781 -> [LTE] network ready
09:57:27.781 -> [OTA] security state loaded=5 current=5 highest=5
09:57:27.814 -> [HTTP] modem HTTP client configured
09:57:27.814 -> [HTTP] GET metadata manifest
09:57:29.559 -> [HTTP] manifest status=200 length=481
09:57:30.087 -> [OTA] no update: manifest version already installed
09:57:30.087 -> [OTA] result=no update
09:57:30.190 -> [APP] version=5 uptime=20
09:57:35.206 -> [APP] version=5 uptime=25
09:57:40.198 -> [APP] version=5 uptime=30
09:59:15.208 -> [APP] version=5 uptime=125
```

署名必須設定のままmanifestの検証を通過し、同じversionを再ダウンロードしなかった。
`loaded=5`はUSB書込み・再起動をまたいで最高適用versionを読み戻せたことを示す。
この確認のための追加の通常RESETは行っていない。

## GUIエクスポート: PASS

「スケッチ」→「コンパイル済みバイナリをエクスポート」を実行し、
スケッチフォルダ配下に次のapplication-only DFU ZIPが生成されることを確認した。

```text
build/SeeedJP.nrf52.wio_bg770a/CellularStatusOta.ino.zip
```

ZIP内のbinaryのSHA-256はUSB書込みに使ったものと一致した。
`.hex`・`.elf`・`.map`も同じフォルダに生成された。
新しいmanifestの署名やSORACOMへのアップロードは行っていない。

## 検証範囲

GUIの検証・USB書込み・シリアルモニタでの実機確認・バイナリエクスポートを行った。
bootloader、署名鍵、SORACOM配信設定は変更していない。
今回はversion 5を維持したため、GUI生成バイナリ同士での新versionへのLTE OTA更新、
障害注入、署名不正などの拒否条件の再試験は行っていない。
Arduino CLI生成バイナリ同士のv4→v5 LTE OTAは前の検証記録を参照する。
