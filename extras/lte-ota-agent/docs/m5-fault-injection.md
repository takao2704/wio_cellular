# M5 障害注入試験

## 目的

LTE OTAの途中で入力や電源が失われても、現在のBank 0アプリが起動し続け、
検証されていないBank 1をbootloaderへ渡さないことを確認する。

試験は危険度の低い順に行う。各ケースの前後で、現在のアプリversion、USB
serialログ、Metadataのmanifest、Harvest Files上のバイナリを記録する。

## 自動試験

```bash
tools/run_native_tests.sh
```

| ケース | 注入内容 | 期待結果 |
|---|---|---|
| manifest構文 | JSON欠損、必須field不正 | manifestを拒否し、downloadを開始しない |
| 対象不一致 | HW識別子不一致、8 bytes未満、397,312 bytes超過 | manifestを拒否する |
| 配信元制限 | HTTPS、許可外host/port、pathなし | URLを拒否する |
| 受信中断 | 宣言sizeの半分で受信終了 | `kImageIncomplete`、verifiedにならない |
| CRC破損 | 計算値と異なるCRC16 | `kCrcMismatch`、verifiedにならない |
| SHA破損 | 計算値と異なるSHA-256 | `kSha256Mismatch`、verifiedにならない |
| 過剰受信 | 宣言sizeを超えるchunk | `kInvalidArgument`、verifiedにならない |

## 実機試験の事前条件

- USB serialで現在のアプリが正常起動する
- resetのダブルクリックでUSB DFUへ入れる
- 現在の正常なアプリZIPをローカルに保存している
- B系の中断試験は、アプリの判断を`kDownloadAndVerify`にするか、意図的にCRC16を
  不一致にしたmanifestを安全ガードとして使う
- MetadataとHarvest Filesの変更前内容を控えている

SWD/J-Linkなしで行う試験は、bootloader settings pageを書き換える前までに限定
する。物理的な電源断は、実施者と切断タイミングを確認してから1ケースずつ行う。

## 実機試験A: manifestとHTTP応答

各ケースでresetしてOTA確認を1回実行し、現在のアプリが再起動後も動くことを
確認する。

| ID | 注入内容 | 期待ログ・状態 |
|---|---|---|
| A1 | manifestの`hardware`を別値にする | manifest field error、firmware GETなし |
| A2 | `size`を397,313にする | manifest field error、Bank 1 eraseなし |
| A3 | firmware URLを許可外hostにする | host rejected、firmware GETなし |
| A4 | Harvest Filesのpathを存在しない値にする | HTTP statusが200以外、適用なし |
| A5 | manifestの`size`とHTTP Content-Lengthを不一致にする | response rejected、Bank 1書込み開始なし |
| A6 | CRC16またはSHA-256だけを改変する | download完了後に検証失敗、適用なし |

### 実測結果

| ID | 実施日 | 結果 | 確認内容 |
|---|---|---|---|
| A1 | 2026-08-30 | PASS | manifestはHTTP 200で取得後に`manifest fields invalid`となり、firmware GETなし。version 2が継続動作。復元後は`application reports no update`を確認 |
| A2 | 2026-08-30 | PASS | `size=397313`を`manifest fields invalid`として拒否し、firmware GETなし。version 2が継続動作し、正常sizeへの復元を確認 |
| A3 | 2026-08-30 | PASS | 許可外hostのURLを`firmware host rejected`として拒否し、firmware GETなし。version 2が継続動作し、正常なHarvest Files URLへの復元を確認 |
| A4 | 2026-08-30 | PASS | 許可されたHarvest Files host内の存在しないpathへGETし、HTTP 404を`firmware response rejected`として拒否。version 2が継続動作し、正常manifestへの復元を確認 |
| A5 | 2026-08-30 | PASS | manifestの129,083 bytesに対しHTTP Content-Lengthが129,084 bytesの応答を`firmware response rejected`として拒否。version 2が継続動作し、正常manifestへの復元を確認 |
| A6 | 2026-08-30 | PASS | CRC16だけを改変。129,084 bytesを受信・Bank 1へ書込み後、`CRC16 mismatch`として有効化を拒否。再起動せずversion 2が継続動作し、正常manifestへの復元を確認 |

## 実機試験B: download中断

更新判断が`kDownloadAndVerify`の検証用アプリ、またはCRC16を意図的に不一致にした
manifestを使う。後者は中断操作が間に合わずdownloadが完了しても、有効化されない
ための安全ガードである。firmware downloadの進捗が出た後、LTE通信を切断するか
電源を切る。Wio BG770Aのreset buttonはセルラーモジュールの電源を切らないため、
通信断試験とWio本体reset試験は別ケースとして扱う。

| ID | 注入内容 | 期待結果 |
|---|---|---|
| B1 | download中にPDP通信を失わせる | read失敗、writer discard、現在アプリ継続 |
| B2 | download中にWio本体をresetする | bootloaderはBank 1を適用せず、現在アプリ起動 |
| B3 | download中に装置全体の電源を切る | 再投入後に現在アプリ起動、次回は先頭から再取得 |

### 実測結果

| ID | 実施日 | 結果 | 確認内容 |
|---|---|---|---|
| B1 | 2026-08-30 | PASS | 397,312 bytesの一時ファイル取得中にSIM sessionを短時間繰り返し削除。HTTP 200取得後のUFS保存が`file store failed`となり、OTAは失敗終了。Bank 1のpayload書込みと有効化を行わずversion 2が継続。正常manifestへの復元、一時ファイル削除、SIMのactive・onlineを確認 |
| B2 | 2026-08-30 | PASS | 32,768 / 129,084 bytesでWio本体をresetし、未完成のBank 1を適用せずversion 2が起動。Metadataの旧値が一度再取得されたため再downloadもresetで中断し、正常manifestで`no update`とversion 2の継続を確認 |
| B3 | 2026-08-30 | PASS | 32,768 / 129,084 bytesで装置全体の電源を切断。再投入後、未完成のBank 1を適用せずversion 2が起動し、正常manifestの`no update`判定を確認 |

現実装は途中再開を行わず、失敗したdownloadを破棄して次回に先頭から取得する。
再試行間隔は`WioOtaAgent`ではなくユーザーアプリが決定する。

BG770A HTTP clientはresponse全体をUFSへ保存してから、MCUがUFSを読み出してBank 1へ
書き込む。そのため、`[OTA] progress`表示後のLTE切断はネットワークreadを中断しない。
B1では397,312 bytesの一時ファイルを使い、`GET firmware image`から
`UFS response stored`までの間に通信断を注入して、UFS保存失敗を確認した。

## 実機試験C: 有効化境界

ここからはbootloader settingsを変更する。正常アプリのUSB DFU復旧を再確認し、
可能ならSWD/J-Linkを用意してから行う。

| ID | 注入内容 | 期待結果 |
|---|---|---|
| C1 | verify完了後、`activate()`前にreset | Bank 1は未適用、現在アプリ起動 |
| C2 | `activate()`完了後、software reset前にreset | bootloaderが検証済みBank 1を適用 |
| C3 | bootloaderがBank 1をBank 0へ反映中に電源断 | 復旧性を記録。通常試験では実施しない |

### 実測結果

| ID | 実施日 | 結果 | 確認内容 |
|---|---|---|---|
| C1 | 2026-08-31 | PASS | M5専用C1ビルドで129,084 bytesの受信とCRC16/SHA-256検証を完了し、`activate()`直前で停止。正常manifestへ戻してresetすると、未登録のBank 1は適用されず、現在のversion 2が起動して`no update`を確認 |
| C2 | 2026-08-31 | PASS | M5専用C2ビルドで検証後にbootloader settingsを登録し、software reset直前で停止。正常manifestへ戻して物理resetすると、登録済みBank 1の処理後に通常version 2が起動して`no update`を確認 |

C1/C2専用環境は`examples/cellular-status-ota`の`ota_v2_m5_c1`と
`ota_v2_m5_c2`を使う。停止ポイントは専用build flagを指定した場合だけ有効であり、
通常の`ota_v1`、`ota_v2`には含まれない。

C3はbootloaderと単一settings pageの限界を調べる破壊的試験である。SWDで全領域を
復旧できる状態にし、別途明示的に実施を決めるまでは行わない。

## M5完了条件

- A1〜A6、B1〜B3、C1〜C2で起動不能にならない
- 不完全または破損したイメージが有効化されない
- 失敗後の次回確認で正常な更新を最初から実行できる
- 各ケースの実測ログと、復旧操作の有無を記録する
- C3を行わない場合は、未検証リスクとしてM6の運用条件に明記する

A1〜A6、B1〜B3、C1〜C2はすべてPASSした。C3は通常のM5受入対象から除外し、
SWD復旧手段を用意した破壊的試験として別管理する。これをもってM5を完了とする。
