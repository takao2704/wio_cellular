# 実機検証記録

ここには試験時の環境、コマンド、実測ログを保存しています。
通常の導入は[PlatformIOガイド](../platformio.md)または[Arduino IDEガイド](../arduino-ide.md)を使ってください。

| 記録 | 確認した内容 |
|---|---|
| [構成整理後のPC上の検証](public-layout-2026-08-31.md) | 回帰テスト、PlatformIO／Arduino CLIビルド、利用例と停止試験の分離 |
| [基盤移行・更新・障害試験](hardware-2026-08-30.md) | bootloader移行、ローカル書込み、LTE OTA、破損・通信断・リセット・電源断 |
| [署名付きOTA](signed-ota-2026-08-31.md) | 署名、旧版拒否、配信率0%／100%、バージョン保存 |
| [Arduino CLIのUSB導入](arduino-usb-2026-08-31.md) | ビルド、手動DFU、LTE接続、更新なし判定 |
| [Arduino CLIのLTE OTA](arduino-ota-2026-08-31.md) | Arduinoビルド間の更新、再起動、更新なし判定 |
| [Arduino IDE GUI](arduino-ide-2026-08-31.md) | コンパイル、USB書込み、更新なし判定、エクスポート |

記録中のアプリ識別値や試験番号は、当時のバイナリとログを照合するために保持しています。
ライブラリのリリース番号ではありません。新しい構成での再実行結果を表すものでもありません。
初期PoCのコードと当時のコマンドは
[整理前のコミット](https://github.com/takao2704/wio_cellular/tree/de0044f74c540a4258d631c5ee6499d431f13840/extras/lte-ota-agent)
で参照できます。削除済みの環境名を現在のツリーで実行しないでください。

実機試験はHW v1.0の1台です。GUI生成バイナリ同士のLTE OTA、HW v1.1、
多台数での配信率分布、bootloaderのコピー・settings書込み中、
バージョン記録書込み中の電源断は未検証です。
停止・リセット試験の現行設定と手順は[障害注入試験](../fault-injection.md)を参照してください。
