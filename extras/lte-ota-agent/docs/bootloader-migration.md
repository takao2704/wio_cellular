# HW v1.0のbootloader確認・移行

OTAライブラリを導入する前に、実機が対応dual-bank bootloaderとS140 7.3.0で動作しているか確認します。
この操作は初回の基盤移行です。通常のアプリ更新で毎回行う必要はありません。

## 現在の状態を確認する

USB接続してRESETをダブルクリックし、DFUモードで表示されるBOOTボリュームの
`INFO_UF2.TXT`を開きます。bootloaderの識別子とSoftDeviceを控えます。
DFUモードに入っただけでは、アプリは書き換わりません。

このプロジェクトのHW v1.0実機で検証した組み合わせは次のとおりです。

- Bootloader: `0.9.1-33-g3cb5c65-dirty`
- SoftDevice: S140 7.3.0
- SeeedJP nRF52 Boards: 1.5.1
- 移行に使ったZIPのSHA-256: `ba308f0f028706225764719d470b6facd7bd5e643f11b1781e5aa6a09106b8a5`

これは検証済みイメージの識別情報です。`0.9.1`や`S140 7.3.0`という表示だけで、
dual-bank対応を判断しないでください。識別子や導入元が異なる場合は、
そのbootloaderのメモリ構成と更新仕様を確認してから利用します。

## Arduino IDEから移行する

既存アプリ領域は移行で失われる前提です。現在のアプリと設定の復旧方法、
対応する書込み用ファイルを用意し、給電が安定した状態で実施してください。
稼働中の製品へ無断で実施しないでください。

1. Arduino IDEへSeeedJP nRF52 Boards 1.5.1を導入します。
   ボードマネージャのURLは`https://www.seeed.co.jp/package_SeeedJP_index.json`です。
2. ボードをWio BG770A、Board Versionを1.0、SoftDeviceをS140 7.3.0に設定します。
3. Programmerに`Bootloader DFU for Bluefruit nRF52`を選び、接続したポートを指定します。
4. 「ツール」→「ブートローダを書き込む」を実行します。
   USB DFUへ自動移行できない場合はRESETをダブルクリックし、DFUポートを選び直します。
5. 完了後、DFUモードで`INFO_UF2.TXT`を確認し、上記の検証済み識別情報と照合します。
6. [Arduino IDE](arduino-ide.md)または[PlatformIO](platformio.md)から、
   OTA処理を含む最初のユーザーアプリを書き込みます。

Core 1.5.1の設定では、この操作に
`Seeed_Wio_BG770A_bootloader-0.9.1_s140_7.3.0.zip`を使います。
同梱ファイルのハッシュが上記と一致することを確認してから実施してください。
異なるパッケージへ同じ手順を無条件に適用しないでください。

bootloader更新だけではLTE OTAは始まりません。
続けて導入するアプリにOTA処理と公開鍵が必要です。
初期のUSB転送試験用アプリを間に挟む必要はありません。

## 失敗時

書込みが完了しない、想定と違う識別情報になる、USB DFUへ戻れない場合は、
OTAの試験へ進まず状態を確認します。USB DFUが使えない場合の復旧にはSWD/J-Link等が必要になることがあります。

[Seeed公式ユーザーマニュアル](https://seeedjp.github.io/Wiki/Wio_BG770A/user-manual.html)と
[移行時の実測記録](validation/hardware-2026-08-30.md)も参照してください。
