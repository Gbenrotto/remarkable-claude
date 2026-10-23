#!/bin/sh
set -eu
rm -f /home/root/xovi/extensions.d/ink-diary.so /home/root/xovi/exthome/qt-resource-rebuilder/inkDiary.qmd
printf 'Plugin removed. Restart the UI to unload it. Configuration/key retained in /home/root/.config/ink-diary.\n'
