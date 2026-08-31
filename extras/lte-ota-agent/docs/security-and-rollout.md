# 署名・アンチロールバック・段階配信

## 保護する範囲

format 2 manifestはEd25519署名を持つ。`WioOtaAgent`はアプリの
`DecisionCallback`を呼ぶ前に署名を検証し、署名対象のSHA-256とdownload後の
ファームウェアを照合する。未署名や署名不正のmanifestはdownload前に拒否する。
署名済みmanifestのSHA-256と異なるイメージは適用しない。ただし、ファームウェア本体の
全体検証はBank 1への書込み後に完了するため、改ざんされた本文が一時的にBank 1へ
書き込まれることはある。検証失敗時はbootloaderへの適用登録を行わない。

署名対象はJSON文字列そのものではない。次の値を固定順序、big-endian、
length-prefix付きのバイナリへ変換する。

- `format`、`hardware`、`version`
- `url`、`size`、`crc16`、`sha256`
- `release_id`、`rollout`、`key_id`

JSONの空白やフィールド順序を変えても署名は有効だが、上記の値を1つでも変更すると
拒否される。`signature`自身は署名対象に含めない。

## 鍵を用意する

検証用の鍵を作る例を示す。本番秘密鍵は開発PCやリポジトリへ常置せず、アクセス制御
された署名環境で扱う。

署名ツールにはPythonの`cryptography`パッケージが必要である。

```bash
openssl genpkey -algorithm ED25519 -out manifest-signing-key.pem
chmod 600 manifest-signing-key.pem
openssl pkey -in manifest-signing-key.pem -pubout -out manifest-public.pem
```

端末へ組み込むのは公開鍵だけである。

```bash
python3 tools/export_manifest_public_key.py manifest-public.pem \
  --key-id production-2026 \
  --output examples/cellular-status-ota/src/ota_manifest_public_key.h
```

生成先は`.gitignore`対象である。公開鍵を製品ソースで管理する場合も、秘密鍵と同じ
ファイルやディレクトリには置かない。

## 署名付きmanifestを作る

`rollout`は0〜10,000のbasis pointで指定する。`2500`は25%、`10000`は全端末を
意味する。`release_id`を変えると端末の抽選結果も変わる。

```bash
python3 tools/firmware_manifest.py firmware.zip \
  --version 5 \
  --url http://harvest-files.soracom.io/wio-bg770a/v5/firmware.bin \
  --signing-key manifest-signing-key.pem \
  --key-id production-2026 \
  --release-id release-5 \
  --rollout 2500 \
  --output manifest-v5.json \
  --firmware-output firmware.bin
```

秘密鍵を指定しない従来コマンドはformat 1を生成する。これは移行試験用であり、
`require_signature = true`の端末は受け付けない。

## アプリへ組み込む

PlatformIOの`examples/cellular-status-ota`では`WIO_OTA_SECURE`をbuild flagに定義すると、
Ed25519実装をリンクし、次の処理を有効にする。PlatformIOの独自アプリでConfigだけを
組み立てる場合は`WIO_OTA_ENABLE_ED25519`をbuild flagへ定義する。

Arduino IDE/CLIではnRF52840向け署名検証実装をライブラリ側で自動的にコンパイルする。
スケッチ内の`#define`は別途コンパイルされるライブラリへ伝わらないためである。
署名を必須にするかはConfigで指定する。共通`.ino`は署名必須が既定であり、
`ota_sketch_config.h`で設定する。公開鍵ヘッダは`src`ではなく`.ino`と同じフォルダへ
生成する。導入とビルド手順は[Arduino IDEガイド](arduino-ide.md)を参照する。

1. `VersionStore::begin()`で不揮発レコードを読む。
2. `setup()`まで到達した現在の`APP_VERSION`を記録する。
3. Ed25519署名、`key_id`、最高適用バージョンを検証する。
4. nRF52840のdevice IDと`release_id`から段階配信bucketを計算する。
5. 合格したmanifestだけをアプリの`DecisionCallback`へ渡す。

```cpp
config.security.require_signature = true;
config.security.manifest_public_key = wio_ota_keys::kManifestPublicKey;
config.security.expected_key_id = wio_ota_keys::kManifestKeyId;
config.security.enforce_anti_rollback = true;
config.security.current_version = APP_VERSION;
config.security.highest_installed_version =
    version_store.highestInstalledVersion();
config.security.enforce_rollout = true;
config.security.rollout_device_id = rollout_device_id;
```

`VersionStore`はInternalFSの2ファイルを交互に更新する。片方が不完全な場合は、
もう片方の正常レコードから読み戻す設計である。書込み中の電源断は実機未検証である。新バージョンをdownloadした時点では記録せず、
更新後のアプリが`setup()`へ到達してから記録するため、適用前の失敗で同じ版の再試行を
妨げない。

署名検証を有効にしたサンプルは、LTE接続後に`security state loaded=... current=...
highest=...`を出力する。`loaded`は今回のversionを書き込む前に不揮発メモリから読んだ値、
`current`は実行中のアプリversion、`highest`は記録後の値である。

InternalFSはnRF52840の`0xED000`〜`0xF4000`を使う。Bank 1の最大終端
`0xE9000`およびbootloader開始`0xF4000`とは重ならない。既存アプリがInternalFSを
使用している場合は同じファイルシステムを共有するため、ファイル名の衝突と初回mount
時の挙動を事前に確認する。

## 段階配信の判定

bucketは次の式で固定する。device IDはmanifestやログへ出さない。

```text
SHA-256(device_id || 0x00 || release_id)の先頭32 bit % 10000
```

bucketが`rollout`未満なら対象、以上なら`Result::kDeferred`になる。同じdevice IDと
`release_id`では常に同じ結果になるため、配信率を25%から50%へ上げると対象集合が
単調に増える。

## 鍵更新と残る制約

現在のConfigは1つの`key_id`と公開鍵を受け付ける。鍵を更新するときは、旧鍵で署名した
ファームウェアに新しい公開鍵を先に組み込み、その版の展開完了後にmanifestの
`key_id`を切り替える。

この署名はLTE OTA経路を保護する。SWDやUSB DFUへ物理アクセスできる攻撃者、
bootloader自体の置換、秘密鍵の漏えいは対象外である。bootloaderは独自にEd25519を
検証しないため、Agentの署名検証を無効にしたビルドを本番配布しない。

## 検証範囲

[署名付きOTAの実機検証記録](validation/signed-ota-2026-08-31.md)と
[検証範囲一覧](validation/README.md)を参照する。
試験結果はHW v1.0の1台で確認した範囲に限る。
多台数での配信率分布、不揮発レコード書込み中やbootloader反映中の電源断は未検証である。
