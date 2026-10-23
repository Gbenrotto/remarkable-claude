#!/bin/sh
set -eu
cd -- "$(dirname -- "$0")"
[ "$(id -u)" = 0 ] || { echo 'Run this on the tablet as root'; exit 1; }
grep -q '^REMARKABLE_RELEASE_VERSION=3.27.3.0$' /usr/share/remarkable/update.conf || { echo 'Requires OS 3.27.3.0'; exit 1; }
[ "$(uname -m)" = armv7l ] || { echo 'Requires reMarkable 2'; exit 1; }
[ -f bin/ink-diary.so ] || { echo 'Build the plugin first'; exit 1; }
/home/root/.vellum/bin/vellum add 'rm-shot>=1.2.0' xovi-message-broker framebuffer-spy qt-resource-rebuilder
base=/home/root/.config/ink-diary
mkdir -p "$base"
chmod 700 "$base"
cp icons/chatgpt.svg "$base/chatgpt.svg"
chmod 644 "$base/chatgpt.svg"
cp fonts/DejaVuSansMono.ttf "$base/DejaVuSansMono.ttf"
chmod 644 "$base/DejaVuSansMono.ttf"
cp fonts/Caveat.ttf "$base/Caveat.ttf"
chmod 644 "$base/Caveat.ttf"
cp bin/config-model "$base/config-model"
chmod 700 "$base/config-model"
cp configure.sh "$base/configure.sh"
chmod 700 "$base/configure.sh"
cp bin/ink-diary.so /home/root/xovi/extensions.d/ink-diary.so.new
chmod 755 /home/root/xovi/extensions.d/ink-diary.so.new
mv /home/root/xovi/extensions.d/ink-diary.so.new /home/root/xovi/extensions.d/ink-diary.so
cp inkDiary.qmd /home/root/xovi/exthome/qt-resource-rebuilder/inkDiary.qmd
chmod 644 /home/root/xovi/exthome/qt-resource-rebuilder/inkDiary.qmd
printf 'Installed. Configure with: sh /home/root/.config/ink-diary/configure.sh\nRestart xochitl through /home/root/xovi/start to load the extension.\n'
