#!/bin/sh
set -eu
cd -- "$(dirname -- "$0")"
[ "$(id -u)" = 0 ] || { echo 'Run this on the tablet as root'; exit 1; }

# Which tablet is this? reMarkable 2 (armv7l) or Paper Pro ("ferrari", aarch64).
# The Paper Pro Move ("chiappa") has a different screen and is not supported.
machine=$( { tr 'A-Z' 'a-z' < /sys/devices/soc0/machine; } 2>/dev/null || true)
arch=$(uname -m)
case "$arch:$machine" in
    armv7l:*"remarkable 2"*) device=rm2; so_class=01 ;;
    aarch64:*ferrari*)        device=paperpro; so_class=02 ;;
    aarch64:*chiappa*)        echo 'The reMarkable Paper Pro Move is not supported (different screen size)'; exit 1 ;;
    *) echo "Requires a reMarkable 2 or a reMarkable Paper Pro (found: $arch, '$machine')"; exit 1 ;;
esac
# OS version: older releases keep it in update.conf; newer ones (Paper Pro on 3.2x) only in
# /etc/os-release as IMG_VERSION="x.y.z.w". Never fatal: it only selects a warning.
firmware=$( { sed -n 's/^REMARKABLE_RELEASE_VERSION=//p' /usr/share/remarkable/update.conf; } 2>/dev/null || true)
[ -n "$firmware" ] || firmware=$( { sed -n 's/^IMG_VERSION=//p' /etc/os-release | tr -d "\"'"; } 2>/dev/null || true)
firmware=${firmware:-unknown}
# Firmware versions this plugin has been run on. Others may work: the plugin checks
# Qt compatibility at load and probes the notebook app at runtime.
case "$device:$firmware" in
    rm2:3.27.3.0) ;;
    paperpro:3.28.0.172) echo 'Paper Pro port: first on-device validation is still pending; see INSTALL-PAPER-PRO.md.' ;;
    *) echo "Warning: Ink Diary has not been tested on OS $firmware for this tablet. Build bin/ink-diary.so with the SDK for this OS version."
       echo "If the notebook app misbehaves, run uninstall.sh and restart the UI." ;;
esac

[ -f bin/ink-diary.so ] || { echo 'Build the plugin first'; exit 1; }
# Refuse a binary built for the other tablet: byte 5 of an ELF header is 01 for 32-bit, 02 for 64-bit.
for binary in bin/ink-diary.so bin/config-model; do
    class=$( (od -An -tx1 -j4 -N1 "$binary" 2>/dev/null || hexdump -s4 -n1 -e '1/1 "%02x"' "$binary" 2>/dev/null) | tr -d ' \n')
    if [ -z "$class" ]; then echo "Warning: cannot verify the architecture of $binary on this system"
    elif [ "$class" != "$so_class" ]; then echo "$binary is not built for this tablet ($device). Rebuild it with the matching SDK."; exit 1; fi
done

# xovi helper extensions: qt-resource-rebuilder (button), xovi-message-broker (QML <-> plugin),
# framebuffer-spy and rm-shot (page screenshot). Use Vellum when it is installed; otherwise
# copy the prebuilt extensions that the Paper Pro bundle carries in deps/.
ext=/home/root/xovi/extensions.d
[ -d /home/root/xovi ] || { echo 'xovi is not installed (/home/root/xovi missing). Install xovi first; see INSTALL.md or INSTALL-PAPER-PRO.md.'; exit 1; }
mkdir -p "$ext"
if [ -x /home/root/.vellum/bin/vellum ]; then
    /home/root/.vellum/bin/vellum add 'rm-shot>=1.2.0' xovi-message-broker framebuffer-spy qt-resource-rebuilder
elif [ "$device" = paperpro ] && [ -f deps/xovi-aarch64.tar.gz ] && [ -f deps/rm-shot-aarch64.so ]; then
    echo 'Vellum not found; installing the helper extensions from deps/.'
    tmp=$(mktemp -d /tmp/ink-diary-deps-XXXXXX)
    trap 'rm -rf "$tmp"' EXIT
    tar -xzf deps/xovi-aarch64.tar.gz -C "$tmp"
    put() { # put SOURCE DEST  (atomic, executable, or stop the install)
        cp "$1" "$2.new" && chmod 755 "$2.new" && mv "$2.new" "$2" || { echo "Failed to install $2 from $1"; exit 1; }
    }
    for name in xovi-message-broker framebuffer-spy; do
        if [ -f "$ext/$name.so" ]; then echo "  $name: already present, kept"; continue; fi
        src="$tmp/xovi/inactive-extensions/$name.so"; [ -f "$src" ] || src="$tmp/xovi/extensions.d/$name.so"
        put "$src" "$ext/$name.so"
        echo "  $name: installed"
    done
    if [ -f "$ext/qt-resource-rebuilder.so" ]; then echo '  qt-resource-rebuilder: already present, kept'
    else
        put "$tmp/xovi/extensions.d/qt-resource-rebuilder.so" "$ext/qt-resource-rebuilder.so"
        mkdir -p /home/root/xovi/exthome/qt-resource-rebuilder /home/root/xovi/services/xochitl.service
        [ -f /home/root/xovi/rebuild_hashtable ] || { cp "$tmp/xovi/rebuild_hashtable" /home/root/xovi/rebuild_hashtable; chmod 755 /home/root/xovi/rebuild_hashtable; }
        [ -f /home/root/xovi/services/xochitl.service/qt-resource-rebuilder.conf ] || cp "$tmp/xovi/services/xochitl.service/qt-resource-rebuilder.conf" /home/root/xovi/services/xochitl.service/
        echo '  qt-resource-rebuilder: installed'
    fi
    if ls "$ext"/rm-shot*.so >/dev/null 2>&1; then echo '  rm-shot: already present, kept (needs 1.2.0 or newer)'
    else put deps/rm-shot-aarch64.so "$ext/rm-shot.so"; echo '  rm-shot: installed'; fi
    rm -rf "$tmp"; trap - EXIT
else
    echo 'Neither Vellum nor the prebuilt helper extensions in deps/ were found.'
    if [ "$device" = paperpro ]; then
        echo 'Install from the Paper Pro bundle (build-paperpro.sh or the GitHub Actions artifact; see INSTALL-PAPER-PRO.md), which carries deps/.'
    else
        echo 'Install Vellum first (INSTALL.md step 2), then rerun install.sh.'
    fi
    exit 1
fi
mkdir -p /home/root/xovi/exthome/qt-resource-rebuilder  # Vellum path: make sure the patch directory exists

base=/home/root/.config/ink-diary
mkdir -p "$base"
chmod 700 "$base"
cp icons/claude.svg "$base/claude.svg"
chmod 644 "$base/claude.svg"
cp fonts/DejaVuSansMono.ttf "$base/DejaVuSansMono.ttf"
chmod 644 "$base/DejaVuSansMono.ttf"
cp fonts/Caveat.ttf "$base/Caveat.ttf"
chmod 644 "$base/Caveat.ttf"
cp bin/config-model "$base/config-model"
chmod 700 "$base/config-model"
cp configure.sh "$base/configure.sh"
chmod 700 "$base/configure.sh"
cp bin/ink-diary.so "$ext/ink-diary.so.new"
chmod 755 "$ext/ink-diary.so.new"
mv "$ext/ink-diary.so.new" "$ext/ink-diary.so"
cp inkDiary.qmd /home/root/xovi/exthome/qt-resource-rebuilder/inkDiary.qmd
chmod 644 /home/root/xovi/exthome/qt-resource-rebuilder/inkDiary.qmd
printf 'Installed for the %s. Configure with: sh /home/root/.config/ink-diary/configure.sh\n' "$device"
printf 'Then run: bash /home/root/xovi/rebuild_hashtable   (once per OS version)\n'
printf 'Restart xochitl through /home/root/xovi/start to load the extension.\n'
