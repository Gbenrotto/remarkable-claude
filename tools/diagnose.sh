#!/bin/sh
# Ink Diary health check, run on the tablet. Prints no secrets (the API key is only reported as present or missing).
exec 2>&1
echo "== device"; uname -m; cat /sys/devices/soc0/machine; sed -n 's/^IMG_VERSION=//p' /etc/os-release; date
echo "== xovi"; ls -l /home/root/xovi/
echo "== active extensions"; ls -l /home/root/xovi/extensions.d/
echo "== inactive extensions"; ls /home/root/xovi/inactive-extensions/ 2>/dev/null
echo "== qt-resource-rebuilder"; ls -l /home/root/xovi/exthome/qt-resource-rebuilder/
echo "== appload apps"; ls /home/root/xovi/exthome/appload/ 2>/dev/null
echo "== ink-diary files"; ls -l /home/root/.config/ink-diary/
c=/home/root/.config/ink-diary/config.json
if [ -f "$c" ]; then
  k=$(sed -n 's/.*"api_key" *: *"\([^"]*\)".*/\1/p' "$c")
  # Only the key's length and type, never any of its characters.
  case "$k" in sk-ant-api03-*) shape="looks like an Anthropic API key" ;; sk-ant-*) shape="Anthropic token, but NOT an API key" ;; *) shape="does NOT look like an Anthropic API key" ;; esac
  echo "api_key: ${#k} characters, $shape (expected: about 108 characters starting sk-ant-api03-)"
  grep -o -E '"(model|timezone|demo_mode|effort)" *: *("[^"]*"|true|false)' "$c"
else echo "config.json: MISSING"; fi
echo "== xochitl"; systemctl is-active xochitl; systemctl show xochitl -p ActiveEnterTimestamp -p NRestarts
for p in $(pidof xochitl); do
  echo "pid $p:"; grep -o '/home/root/xovi[^ ]*\.so' /proc/$p/maps | sort -u
done
echo "== journal (this boot, filtered)"
journalctl -u xochitl -b --no-pager | grep -v '\[QTFB\]' | grep -i -E 'ink-diary|qmldiff|xovi|appload|rm-shot|broker|framebuffer-spy|resource|hashtab|fatal|segfault|abort|not loading|qml.*(error|warning)|DocumentView' | tail -n 60
echo "== probe"
if [ -p /run/xovi-mb ]; then timeout 5 sh -c "echo '>eink-diary\$probe:' > /run/xovi-mb; cat /run/xovi-mb-out"; echo
else echo "no message-broker pipe (/run/xovi-mb): the broker extension is not running"; fi
echo "== done"
