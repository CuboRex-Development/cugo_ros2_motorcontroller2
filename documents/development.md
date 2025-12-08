# Development Guide

開発者向けドキュメントです。

## プロジェクト構成

```
.
├── src/                    # メインアプリケーション
│   └── main.cpp
├── lib/                    # ライブラリ
│   ├── CugoSDK/           # ハードウェア制御（Pico専用）
│   ├── MotorLogic/        # モーター制御ロジック（テスト可能）
│   └── SerialProtocol/    # 通信プロトコル（テスト可能）
├── test/                   # ユニットテスト
│   ├── test_motor_logic/
│   └── test_serial_protocol/
└── platformio.ini          # PlatformIO設定
```

## 環境構築

### PlatformIOのインストール

```bash
pip install platformio
```

または、VS Codeの拡張機能「PlatformIO IDE」をインストール。

## ビルド

```bash
# Raspberry Pi Pico用にビルド
pio run -e pico

# 生成されるファイル: .pio/build/pico/firmware.uf2
```

## テスト

```bash
# ユニットテストを実行（PC上で実行）
pio test -e native
```

テストはPC上で実行され、ハードウェアは不要です。

### テスト対象

| ライブラリ | テスト内容 |
|-----------|-----------|
| MotorLogic | RPMクランプ処理、製品ID判定 |
| SerialProtocol | チェックサム計算、バッファ操作 |

## 書き込み

```bash
# ビルドして書き込み（Picoを接続した状態で）
pio run -e pico -t upload

# シリアルモニタ
pio device monitor
```

## リリース

新しいバージョンをリリースする手順：

```bash
# 1. mainブランチを最新に
git checkout main
git pull

# 2. タグを作成（セマンティックバージョニング）
git tag v1.0.0

# 3. タグをpush
git push --tags
```

タグをpushすると、GitHub Actionsが自動的に：
1. ファームウェアをビルド
2. GitHub Releaseを作成
3. `.uf2`ファイルを添付

## CI/CD

| ワークフロー | トリガー | 内容 |
|-------------|---------|------|
| Test | main へのpush/PR | ユニットテスト + ビルド確認 |
| Release | `v*` タグpush | ファームウェアビルド + Release作成 |

## ライブラリ構成

### CugoSDK
- Raspberry Pi Pico専用のハードウェア制御
- モータードライバ（LD-2）との通信
- エンコーダ値の取得

### MotorLogic
- `clamp_rpm_simple()`: 単純なRPM上限クランプ
- `clamp_rpm_rotation_priority()`: 回転成分を優先したクランプ
- `check_max_rpm()`: 製品IDに基づく最大RPM取得

### SerialProtocol
- `calculate_checksum()`: パケットのチェックサム計算
- `write_*_to_buf()` / `read_*_from_buf()`: バッファ操作
- `create_serial_packet()`: パケット生成
