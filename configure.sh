#!/bin/sh
set -eu
umask 077
# A timezone is an IANA name the tablet has zone data for, or a fixed offset written
# exactly as +HH:MM / -HH:MM / +HHMM (what the plugin's parser accepts).
tz_check() {
    case "$1" in
        [+-][01][0-9]:[0-5][0-9]|[+-][01][0-9][0-5][0-9]) return 0 ;;
        [+-]*) echo "Write a fixed offset as +HH:MM, e.g. +09:30 for Darwin"; return 1 ;;
        ''|*[!A-Za-z0-9_/+-]*) echo 'Invalid timezone name'; return 1 ;;
    esac
    [ -f "/usr/share/zoneinfo/$1" ] || { echo "This tablet has no zone data for $1. Use a fixed UTC offset instead, e.g. +09:30 for Darwin."; return 1; }
}
if [ "${1:-}" = --model ]; then
    [ "$#" = 2 ] || { echo 'Usage: configure.sh --model MODEL'; exit 1; }
    exec /home/root/.config/ink-diary/config-model "$2"
fi
if [ "${1:-}" = --gmail ]; then
    [ "$#" = 1 ] || { echo 'Usage: configure.sh --gmail'; exit 1; }
    tool=/home/root/.config/ink-diary/config-model
    printf 'Gmail address: ' > /dev/tty
    IFS= read -r gmail_address < /dev/tty
    printf 'Gmail app password (hidden, from myaccount.google.com/apppasswords): ' > /dev/tty
    stty -echo < /dev/tty
    trap 'stty echo < /dev/tty' EXIT HUP INT TERM
    IFS= read -r gmail_password < /dev/tty
    stty echo < /dev/tty
    printf '\nFirst name for signing replies (optional): ' > /dev/tty
    IFS= read -r sign_name < /dev/tty
    printf '%s\n' "$gmail_address" | "$tool" --set gmail_address
    printf '%s\n' "$gmail_password" | "$tool" --set gmail_app_password
    unset gmail_password
    [ -z "$sign_name" ] || printf '%s\n' "$sign_name" | "$tool" --set name
    exit 0
fi
if [ "${1:-}" = --timezone ]; then
    [ "$#" = 2 ] || { echo 'Usage: configure.sh --timezone Area/City   (IANA name, e.g. Australia/Darwin)'; exit 1; }
    tz_check "$2" || exit 1
    printf '%s\n' "$2" | /home/root/.config/ink-diary/config-model --set timezone
    exit $?
fi
[ "$#" = 0 ] || { echo 'Usage: configure.sh [--model MODEL | --gmail | --timezone Area/City]'; exit 1; }
base=/home/root/.config/ink-diary
mkdir -p "$base"
if [ -f "$base/config.json" ]; then
    printf 'Note: this replaces the whole configuration (Gmail, effort and other settings included).\nTo change one setting keep the rest, use --model, --timezone or --gmail instead. Press Ctrl-C to stop.\n' > /dev/tty
fi
printf 'Anthropic API key (hidden): ' > /dev/tty
stty -echo < /dev/tty
trap 'stty echo < /dev/tty' EXIT HUP INT TERM
IFS= read -r diary_key < /dev/tty
stty echo < /dev/tty
printf '\n' > /dev/tty
case "$diary_key" in ''|*[!A-Za-z0-9_-]*) echo 'Invalid key format'; exit 1;; esac
printf 'Model [claude-sonnet-5-5]: ' > /dev/tty
IFS= read -r diary_model < /dev/tty
diary_model=${diary_model:-claude-sonnet-5-5}
case "$diary_model" in *[!A-Za-z0-9_.-]*) echo 'Invalid model name'; exit 1;; esac
printf 'Timezone for dates and web answers [Australia/Darwin]: ' > /dev/tty
IFS= read -r diary_tz < /dev/tty
diary_tz=${diary_tz:-Australia/Darwin}
tz_check "$diary_tz" || exit 1
printf '{"api_key":"%s","model":"%s","timezone":"%s"}\n' "$diary_key" "$diary_model" "$diary_tz" > "$base/config.json.new"
mv "$base/config.json.new" "$base/config.json"
unset diary_key
printf 'Configuration saved. No UI restart needed.\n'
