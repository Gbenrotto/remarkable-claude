#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
: "${SDK_ENV:?Set SDK_ENV to the environment-setup file of the official rM2 SDK matching your OS version}"
: "${XOVI_REPO:?Set XOVI_REPO to the xovi source checkout}"
set +u
source "$SDK_ENV"
set -u
python3 "$XOVI_REPO/util/xovigen.py" -o xovi.cpp -H xovi.h ink-diary.xovi
test -s xovi.cpp && test -s xovi.h
mkdir -p build
cd build
qmake ../ink-diary.pro CONFIG+=release
make -j"${JOBS:-4}"
mkdir -p ../bin
cp ink-diary.so ../bin/ink-diary.so
$STRIP --strip-unneeded ../bin/ink-diary.so

$CXX -std=c++17 -fPIC -ffile-prefix-map="$HOME"=/build-home ../src/config-model.cpp -o ../bin/config-model $(pkg-config --cflags --libs Qt6Core)
$STRIP --strip-unneeded ../bin/config-model
