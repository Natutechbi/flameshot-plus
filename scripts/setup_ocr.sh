#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
VENV_DIR="$REPO_DIR/venv-ocr"
DATA_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/flameshot/venv-ocr"

echo "==> Setting up PaddleOCR for Flameshot+"
echo "    Venv: $VENV_DIR"

# Create virtual environment
python3 -m venv "$VENV_DIR"

# Upgrade pip
"$VENV_DIR/bin/pip" install --upgrade pip

# Install PaddlePaddle (CPU) and PaddleOCR
"$VENV_DIR/bin/pip" install paddlepaddle paddleocr

echo ""
echo "==> Creating symlink at $DATA_DIR"
mkdir -p "$(dirname "$DATA_DIR")"
ln -sfT "$VENV_DIR" "$DATA_DIR"

echo ""
echo "==> Done! PaddleOCR is ready."
echo "    To use: run Flameshot and press 'O' during a capture."
