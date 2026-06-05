#!/usr/bin/env bash
set -euo pipefail

WORKSPACE="${ZEPHYR_WORKSPACE:-$HOME/projects/zephyrproject}"
APP_DIR="$(cd "$(dirname "$0")" && pwd)"
IMAGE="${ZEPHYR_BUILD_IMAGE:-ghcr.io/zephyrproject-rtos/zephyr-build:main}"
BOARD="${BOARD:-psdr_f7}"
PRISTINE="${PRISTINE:-auto}"

if [ ! -d "$WORKSPACE/.west" ]; then
  echo "No west workspace at $WORKSPACE. See stm32f405_base_app/BUILD.md for setup." >&2
  exit 1
fi

exec docker run --rm \
  -v "$WORKSPACE":/workdir/zephyrproject \
  -v "$APP_DIR":/workdir/psdr_f7_app \
  "$IMAGE" \
  bash -c "
    git config --global --add safe.directory '*'
    cd /workdir/zephyrproject
    west zephyr-export >/dev/null
    west build -b $BOARD -p $PRISTINE /workdir/psdr_f7_app
  "
