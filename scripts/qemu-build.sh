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

# 测试目录
TEST_DIR="/c/Users/xidon/code/github/qemu-embedded-platform/testcase"

echo "==> Enter QEMU source directory"
cd "${QEMU_SRC}"

echo "==> Creating configure/build directories"
mkdir -p "${CONFIG_DIR}"
mkdir -p "${BUILD_DIR}"

cd "${CONFIG_DIR}"

if [[ -f "config-host.mak" && -f "Makefile" ]]; then
    echo "==> Configure already completed, skipping configure step"
else
    echo "==> Running configure"
    ../configure \
        --prefix="${BUILD_DIR}" \
        --target-list=arm-softmmu \
        --enable-debug \
        --disable-docs
fi

echo "==> Building QEMU"
make -j16
echo "==> Build finished successfully"

echo "==> QEMU install prefix: ${BUILD_DIR}"
make install
echo "==> Install finished"

echo "==> Running testcase starting"
${BUILD_DIR}/qemu-system-arm.exe -machine mps2-an505 -cpu cortex-m33 -m 16M -kernel ${TEST_DIR}/an505-qemu.elf -display sdl -serial stdio
echo "==> Running testcase finished"
