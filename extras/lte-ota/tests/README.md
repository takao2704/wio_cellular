# 回帰テストと実機停止試験

## PC上の回帰テスト

このOTAサンプルのルート（`extras/lte-ota`）で実行します。
C++17コンパイラ、Python 3、ripgrep、ArduinoJson 7.0.4とPythonの `cryptography` が必要です。
先に[README](../README.md)の鍵生成・PlatformIOビルドを済ませてください。

```bash
PYTHON=.venv/bin/python tools/run_native_tests.sh
```

スクリプトは `examples/CellularStatusOta/.pio/libdeps` のArduinoJsonを使います。
別の導入先なら `ARDUINOJSON_INCLUDE_DIR=/path/to/ArduinoJson/src` を指定できます。
C++でCRC16、SHA-256、受信イメージ、manifest、旧版拒否・配信対象の判定を確認します。
Pythonで署名生成、公開鍵ヘッダ、単一Arduino ZIP、ビルドフラグ、公開ファイル構成を確認します。
PC上のC++テストではCC310そのものや実Flash書込みは実行しません。
署名境界だけをテスト用実装へ差し替え、format 2の署名成功・失敗と、署名検証後にだけ判定コールバックを呼ぶ順序を確認します。
`test_runtime.cpp` は実際のAgent・HTTP Client・Writer・VersionStoreを、
`runtime_fakes.h` の模擬モデム・RAM上のFlash・ファイルシステムへ接続して実行します。
拒否・更新なし・延期・取得検証のみ・同一Agentの再利用、UFSの遅延完了URC、
失敗後のclose・delete結果、HTTP設定・GET/READ・ファイル操作の遅延応答を伴うタイムアウト、
再利用禁止状態とモデム停止後の再試行、
保存記録の破損・読出し失敗・マウント失敗・片方の記録からの復旧を確認します。
Coreの自動フォーマット経路が呼ばれていないことも確認対象です。
模擬環境は実際のモデムのURC順序、Flashの電源断耐性、ファイルシステム全体の耐障害性を保証しません。
モデム上での実際のファイルハンドル解放や、CC310の暗号実装は、このホスト試験の対象外です。
テスト専用の `WIO_OTA_NATIVE_TEST` は通常ファームウェアに指定できません。
Coreヘッダ名の代替は `.pio/native-tests` 内だけに生成し、配布ライブラリには含めません。
GitHub Actionsへの自動実行は組み込んでいません。

## 実機試験の準備

以下は保守担当者が明示的に実行する試験です。通常の利用には不要です。
対象端末の正常起動とUSB DFUでの書戻し手段を確認し、試験専用SIMグループに限定します。
Metadata・配信ファイルの変更前の内容を保存し、電源・通信の切断タイミングを実施者と確認してください。

通信断と装置全体の電源断は別です。Wio本体のRESETではセルラーモジュールはリセットされません。
ダウンロード中断試験には `kDownloadAndVerify` を返すアプリなどを使い、
操作が遅れて受信が完了しても適用しない状態にしてください。

| 注入内容 | 確認すること |
|---|---|
| HW・サイズ・許可ホスト不一致 | manifestを拒否し取得しない |
| URL不存在・Content-Length不一致 | 応答を拒否し適用しない |
| CRC16・SHA-256不一致 | 全量取得しても適用しない |
| 署名後のmanifest改変 | 署名検証で拒否し取得しない |
| 正しく署名した旧版 | アンチロールバック有効時に拒否 |
| HTTP取得中のLTE通信断 | 取得に失敗し適用登録しない |
| Bank 1書込み中のRESET・電源断 | 不完全なイメージを適用せず現在アプリが起動 |

署名済みmanifestのCRC等を書き換えると、本文取得前に署名で拒否されます。
試験対象の層に合わせて再署名するか、隔離した試験端末でのみ署名なし設定を使います。
BG770Aは本文全体をUFSへ保存してからMCUへ渡します。LTE通信断は
`GET firmware image` から `UFS response stored` までの間に行います。
進捗ログが出た後ではLTEを切断してもUFS読出しは続きます。再試行は先頭からです。

## 適用登録の前後で止める

専用設定は[hardware/platformio.ini](hardware/platformio.ini)です。
通常のサンプルには停止フラグを含めません。

```bash
pio run -d tests/hardware -e before_activation
pio run -d tests/hardware -e after_activation
```

これらは同じ `.ino` を `APP_VERSION=2` でビルドする、**署名なしの隔離試験用設定**です。
実際の端末と配信manifestの版に合わせて変更してください。本番配布には使いません。
このコマンドだけでは書込みも電源操作も行いません。

| 環境 | 停止地点 | RESET後の期待結果 |
|---|---|---|
| `before_activation` | イメージ検証後、登録前 | 現在アプリが起動 |
| `after_activation` | 登録成功後、再起動前 | bootloaderが更新を反映 |

1. 復旧手段を確認した端末へ、選んだ試験用アプリを手動で書き込みます。
2. 検証が成功する更新イメージとmanifestを試験グループに配置します。
3. `[OTA TEST]` の停止ログを確認します。
4. 再起動後に更新を繰り返さないよう、Metadataを更新なしになる正常値へ戻します。
5. 通常RESETを押し、起動したアプリと適用結果を記録します。

停止フラグは `WIO_OTA_TEST_HALT_BEFORE_ACTIVATE` と `WIO_OTA_TEST_HALT_AFTER_ACTIVATE` です。
指定したビルドは意図的に停止します。通常ビルドには停止処理は入りません。
この試験はbootloaderコピー中やsettings・不揮発レコード書込み中の電源断を検証しません。
その試験には別途承認とSWD等の全領域復旧手段が必要です。

旧構成での[実測記録](https://github.com/takao2704/wio_cellular/blob/873ded2438b8083a7f001cde241c6e2962187a54/extras/lte-ota-agent/docs/validation/README.md)はforkの固定コミットから参照できます。
この単一ライブラリ構成への変更後は、ビルド・PCテストと実機再検証を分けて扱います。
