# 回帰テスト

OTAプロジェクトのルートで実行します。C++17コンパイラ、Python 3、ripgrep、
ArduinoJson 7.0.4とPythonの`cryptography`が必要です。

```bash
python3 -m venv .venv
.venv/bin/python -m pip install cryptography
pio run -d examples/cellular-status-ota -e initial
PYTHON=.venv/bin/python tools/run_native_tests.sh
```

スクリプトは通常のexampleが導入したArduinoJsonを使います。
別環境のArduinoJsonを使う場合は`ARDUINOJSON_INCLUDE_DIR=/path/to/ArduinoJson/src`を指定できます。
初期PoCアプリやOTAプロジェクト直下のPlatformIO設定には依存しません。

C++でCRC16、SHA-256、受信イメージ、manifest、旧版拒否・配信対象の判定を確認します。
Pythonで署名生成、公開鍵ヘッダ、Arduino ZIP、ビルドフラグと公開ファイル構成を確認します。
PC上のC++テストではCC310の実署名検証やFlash書込みは実行しません。

[実機停止試験](../docs/fault-injection.md)は別途明示的に実行します。
現在のリポジトリのGitHub Actionsへ、このテストの自動実行は組み込んでいません。
