#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
: "${SDK_ENV:?Set SDK_ENV to the environment-setup file of the official reMarkable SDK (rm2 or ferrari) matching your OS version}"
: "${XOVI_REPO:?Set XOVI_REPO to the xovi source checkout}"
set +u
source "$SDK_ENV"
set -u
# XOVI_ARCH: arm32 for the reMarkable 2 SDK, aarch64 for the Paper Pro (ferrari) SDK.
python3 "$XOVI_REPO/util/xovigen.py" -a "${XOVI_ARCH:-arm32}" -o xovi.cpp -H xovi.h ink-diary.xovi
test -s xovi.cpp && test -s xovi.h
mkdir -p build
cd build
qmake ../ink-diary.pro CONFIG+=release
make -j"${JOBS:-4}"
mkdir -p ../bin
cp ink-diary.so ../bin/ink-diary.so
$STRIP --strip-unneeded ../bin/ink-diary.so
file ../bin/ink-diary.so 2>/dev/null || true

$CXX -std=c++17 -fPIC -ffile-prefix-map="$HOME"=/build-home ../src/config-model.cpp -o ../bin/config-model $(pkg-config --cflags --libs Qt6Core)
$STRIP --strip-unneeded ../bin/config-model
