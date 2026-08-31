# 公開構成の整理後の検証（2026-08-31）

初期PoCの分離、利用例と停止試験の設定分離、文書の再配置後にPC上で確認した結果です。
この変更後の実機書込み、LTE OTA、電源断試験は実施していません。
以前の実機結果は[検証一覧](README.md)にある当時の記録を参照してください。

## 確認結果

| 対象 | 結果 |
|---|---|
| C++の5テスト実行ファイル | PASS |
| Pythonの署名・Arduino配布・公開構成の計10テスト | PASS |
| PlatformIOの通常サンプル2構成 | PASS |
| PlatformIOの署名必須2構成 | PASS |
| PlatformIOの適用前／適用後停止の2構成 | PASS |
| Arduino CLIの共通スケッチ（署名必須） | PASS |
| Arduino IDE用の3ライブラリZIP生成 | PASS |
| 相対Markdownリンクと差分の空白チェック | PASS |

PlatformIO 6.1.18、固定コミットのSeeedJPプラットフォーム／Core、
WioCellular 0.3.15、ArduinoJson 7.0.4でビルドしました。
Arduino CLI 1.2.2、SeeedJP nRF52 Boards 1.5.1でもビルドしました。
CLIにはこのツリーの3ライブラリを明示して渡し、生成された依存ファイルから
現在のソースをコンパイルしたことを確認しました。IDE共用のインストール済みライブラリは変更していません。

通常のPlatformIOビルドはFlash 139,420 bytes／RAM 21,192 bytes、
署名付きビルドはFlash 172,884 bytes／RAM 21,260 bytesでした。
Arduino CLIはFlash 172,352 bytes／RAM 21,264 bytesでした。
Flashのビルド表示は配信するraw binaryのサイズとは別です。

## 再実行

- PC上の一括検証は[回帰テスト](../../tests/README.md)
- 通常・署名付きビルドは[PlatformIOガイド](../platformio.md)
- Arduino CLIのビルドは[Arduinoガイド](../arduino-ide.md#arduino-cli)
- 停止試験用ビルドは[障害注入試験](../fault-injection.md)

署名付きPlatformIO設定はガイドのローカル設定例を使用しました。
公開鍵ヘッダとその設定ファイルはGit管理対象外です。
通常ビルドのraw binaryには停止ログがなく、専用ビルドには各停止地点の
`[OTA TEST]`ログが入ることを確認しました。停止地点そのものは実機では再試験していません。

移動した過去の記録は、文書リンクと履歴の案内を除いて保持しています。
初期PoCの削除や環境名の変更は、既存アプリ向けのOTA APIを変更しません。
