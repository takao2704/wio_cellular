# M6 実機検証記録（2026-08-31）

この文書は実施当時の記録です。旧環境名・コマンド・実測ログを保持しています。
現行の導入手順と検証範囲は[検証一覧](README.md)から参照してください。

## 試験条件

- Wio BG770A HW v1.0、dual-bank bootloader導入済み
- 実装commit: `9f21bf9`。加えてサンプルに`ota_v3`ビルド環境を追加
- `examples/cellular-status-ota`を`WIO_OTA_SECURE`有効でビルド
- 署名必須、アンチロールバック、段階配信の各ポリシーを有効化
- MetadataとHarvest FilesはFQDNでアクセス
- Metadataは有効かつread-only。対象グループのSIMは試験用1台のみ
- 試験用Ed25519秘密鍵はローカルのGit管理対象外に保存し、端末には公開鍵のみ組込み

v2/v3はそれぞれ`ota_v2`、`ota_v3`環境でビルドした。`APP_VERSION`の重複定義はない。
両方ともFlash 172,732 bytes、RAM 21,260 bytesだった。

## 1. 未署名manifestの拒否: PASS

署名検証を有効にしたv2をUSB DFUで導入した。既存のformat 1、version 2のmanifestを
残して起動し、次を確認した。

```text
[HTTP] manifest status=200 length=237
[OTA] rejected: signed manifest required
[OTA] result=rejected
[APP] version=2 uptime=22
[APP] version=2 uptime=27
[APP] version=2 uptime=32
```

ファームウェア取得へ進まず、通常アプリの動作が継続した。

## 2. 署名付きv2→v3のLTE OTA: PASS

v3のイメージに対するformat 2 manifestを生成した。`rollout=10000`（100%）、
`release_id=m6-test-release-3`で署名し、PC上でも公開鍵による検証に成功した。

Harvest Filesへアップロード後、APIでダウンロードして次のSHA-256一致を確認した。
その後にMetadataの`userdata`だけを差し替え、読み戻したmanifestが生成物と完全一致し、
Metadataがread-onlyのままであることを確認した。

```text
image size: 172732 bytes
CRC16: ae98
SHA-256: a541cbac7003a1b71df7dcdfd474ee607d0c363bfb20c31472089ae705c9915a
```

v2を通常resetして取得したログの抜粋:

```text
[HTTP] manifest status=200 length=449
[OTA] downloading version=3 size=172732
[HTTP] firmware status=200 length=172732
[OTA] progress 16384/172732
[OTA] progress 32768/172732
[OTA] progress 65536/172732
[OTA] progress 98304/172732
[OTA] progress 131072/172732
[OTA] progress 163840/172732
[OTA] progress 172732/172732
[OTA] image verified
[OTA] activated; rebooting
```

`WioOtaAgent`は署名検証成功後にのみdownloadへ進む。イメージ全体の検証後、
bootloaderへの適用登録と自動再起動が行われた。

## 3. 更新後のv3動作と同一version判定: PASS

OTAによる自動再起動後、同じ署名manifestを再取得して次を確認した。

```text
[LTE] network ready
[HTTP] manifest status=200 length=449
[OTA] no update: manifest version already installed
[OTA] result=no update
[APP] version=3 uptime=17
```

v3が起動し、同じversionを再downloadしないことを確認した。この結果だけでは、
不揮発レコードからversion 3を再読出しできたことの独立した証明にはならない。
判定には現在実行中の`APP_VERSION`も使われるため、永続化の確認は別試験で扱う。

## 4. 署名後のmanifest改ざん拒否: PASS

正常なv3 manifestの`release_id`だけを`m6-test-release-3-tampered`へ変更し、
署名は元のままとした。PC上で元manifestの署名成功と、変更後の署名失敗を確認した。
Metadataを変更後のmanifestへ切り替え、APIで読み戻して完全一致を確認してから
v3を通常resetした。

```text
[HTTP] manifest status=200 length=458
[OTA] rejected: manifest signature invalid
[OTA] result=rejected
[APP] version=3 uptime=18
[APP] version=3 uptime=23
```

ファームウェア取得には進まず、v3の動作が継続した。同一versionの更新なし判定より
前に署名検証が行われ、署名不正として拒否されることも確認した。

## 5. 正しく署名された旧versionの拒否: PASS

試験起点とした署名検証有効v2のイメージについて、version 2のformat 2 manifestを
生成した。PCで署名を検証し、Harvest Filesへ配置したイメージのSHA-256一致を
確認してからMetadataを切り替えた。署名は正常なまま、実行中のversion 3より古い
version 2を提示して通常resetした。

```text
[HTTP] manifest status=200 length=449
[OTA] rejected: manifest version rejected by anti-rollback policy
[OTA] result=rejected
[APP] version=3 uptime=16
[APP] version=3 uptime=21
```

署名検証後のアンチロールバック判定で拒否され、ファームウェア取得へ進まなかった。
通常アプリはv3のまま動作した。

## 6. 配信率0%での延期: PASS

正しく署名したversion 4のmanifestを`rollout=0`で提示した。実行中のv3より新しい
versionでも、段階配信の対象外ならdownloadしないことを通常reset後に確認した。

```text
[HTTP] manifest status=200 length=445
[OTA] deferred: device not selected for rollout
[OTA] result=deferred
[APP] version=3 uptime=15
[APP] version=3 uptime=20
[APP] version=3 uptime=25
```

ファームウェア取得には進まず、v3の動作が継続した。署名不正の拒否とは区別され、
アプリには再試行可能な`Result::kDeferred`が返った。

## 7. 配信率100%への変更とv3→v4更新: PASS

後続試験用に`ota_v4`環境を追加し、署名検証有効でビルドした。Flash 172,852 bytes、
RAM 21,260 bytes。v4は、起動時に不揮発メモリから読んだversionを保存し、LTE接続後に
現在version・記録後の最高versionとともに出力する。これにより書込み前の読出し値を
実機ログで確認できる。native testsと署名検証を有効にしない通常v2のビルドも成功した。

v4の署名manifestを配信率0%と100%で生成し、PC上で両方の署名を検証した。0%での
延期を確認した後、同じイメージ・`release_id`のまま100%へ変更した。
変更するフィールドは`rollout`と再生成した`signature`のみである。Metadataの
読み戻し一致とread-only維持を確認し、v3を通常resetした。

```text
image size: 172852 bytes
CRC16: 3585
SHA-256: 2f44fcad1daa86204b7ccf1d0b66dc35a95b48ab9a848fcb8ed9cf2eedc8ee2f

[HTTP] manifest status=200 length=449
[OTA] downloading version=4 size=172852
[HTTP] firmware status=200 length=172852
[OTA] progress 16384/172852
[OTA] progress 65536/172852
[OTA] progress 131072/172852
[OTA] progress 172852/172852
[OTA] image verified
[OTA] activated; rebooting
```

自動再起動後:

```text
cellular-status + OTA, app-version=4
[LTE] network ready
[OTA] security state loaded=3 current=4 highest=4
[HTTP] manifest status=200 length=449
[OTA] no update: manifest version already installed
[OTA] result=no update
[APP] version=4 uptime=17
[APP] version=4 uptime=22
```

配信率100%への変更後は更新が実行され、v4の起動を確認した。`loaded=3`は今回の
version記録前に不揮発メモリから読んだ値であり、v3で保存した状態がOTA適用・再起動を
またいで残ったことを示す。続く`highest=4`は、現在version 4の書込みと読み戻し検証が
成功した後の値である。

## 8. 再起動後のversion 4再読出し: PASS

v4を通常resetし、次を確認した。

```text
[LTE] network ready
[OTA] security state loaded=4 current=4 highest=4
[HTTP] manifest status=200 length=449
[OTA] no update: manifest version already installed
[OTA] result=no update
[APP] version=4 uptime=16
[APP] version=4 uptime=21
[APP] version=4 uptime=26
```

新しく保存したversion 4を、次の起動で`recordCurrentVersion()`より前に再読出し
できた。同一versionの再downloadは起きず、v4の通常動作が継続した。

## 結果と試験終了時の状態

本計画のM6受入試験（1〜8）はすべてPASSとする。端末は署名検証を有効にしたv4で
動作し、Metadataはこのv4に合う正常な署名manifest（配信率100%）になっている。
APIで生成manifestとの完全一致、Metadataのenabled/read-only、Harvest Filesの有効化、
対象グループがactiveな試験SIM 1台のみであることを再確認した。

試験用の署名鍵・生成manifest・DFU ZIPはGit管理対象外に置いている。試験イメージは
Harvest Filesの専用パスに残し、本番鍵や本番配信への切り替えは行っていない。

実機の段階配信試験は1台で0%と100%の境界を確認したものであり、多台数での配信率分布を
検証したものではない。また、bootloader copy中の電源断（M5 C3）や不揮発レコードの
書込み中電源断は今回のM6実機試験に含まない。物理DFU/SWDやbootloader自体の保護も
対象外であり、M6完了を製品全体のセキュリティ保証とはしない。
