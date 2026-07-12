#!/usr/bin/env bash
set -e

if [[ "${MSYSTEM}" != "MINGW64" ]]; then
    echo "Please run this script in MSYS2 MinGW64 shell"
    exit 1
fi

# QEMU源码目录
QEMU_SRC="/c/Users/xidon/code/github/qemu-embedded-platform/qemu"

# 构建目录
CONFIG_DIR="${QEMU_SRC}/qemu-configure"
BUILD_DIR="${QEMU_SRC}/qemu-build"

echo "==> Enter QEMU source directory"
cd "${QEMU_SRC}"

echo "==> Creating configure/build directories"
mkdir -p "${CONFIG_DIR}"
mkdir -p "${BUILD_DIR}"

echo "==> Running configure"
cd "${CONFIG_DIR}"

../configure \
    --prefix="${BUILD_DIR}" \
    --target-list=arm-softmmu \
    --enable-debug \
    --disable-docs

echo "==> Building QEMU"
make -j16

echo "==> Build finished successfully"
echo "QEMU install prefix: ${BUILD_DIR}"
