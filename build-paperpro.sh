#!/usr/bin/env bash
# Build Ink Diary for the reMarkable Paper Pro (aarch64) on Linux or in WSL (Ubuntu), and
# bundle everything install.sh needs into dist/ink-diary-paperpro.tar.gz.
#
#   bash build-paperpro.sh            # full build: SDK download (~1 GB, once), build, bundle
#   bash build-paperpro.sh --bundle   # re-bundle the last build without compiling again
#
# Nothing tracked in this checkout is modified: the compile and the bundle are staged under
# $WORK on the Linux filesystem. That keeps the reMarkable 2 binaries in bin/ intact, avoids
# the case-insensitive Windows drive (the repo's VERSION file would shadow the C++ <version>
# header), and gives the archive proper Linux file modes.
#
# For another OS version, take the ferrari aarch64 toolchain link for it from
# https://developer.remarkable.com/links and run:  OS_VERSION=x.y.z.w SDK_URL=<link> bash build-paperpro.sh
set -euo pipefail
here=$(cd -- "$(dirname -- "$0")" && pwd)
OS_VERSION=${OS_VERSION:-3.28.0.172}
SDK_URL=${SDK_URL:-https://storage.googleapis.com/remarkable-codex-toolchain/3.28.0.172/remarkable-production-image-5.8.203-ferrari-public-aarch64-toolchain.sh}
SDK_DIR=${SDK_DIR:-$HOME/rm-sdk/ferrari-$OS_VERSION}
SDK_INSTALLER=${SDK_INSTALLER:-$HOME/rm-sdk/ferrari-$OS_VERSION-toolchain.sh}
WORK=${WORK:-$HOME/ink-diary-build}
EXPECT_ARCH=${EXPECT_ARCH:-ARM aarch64}     # what `file` must report for the built binaries
# Pinned inputs, so a build today and a build next month produce the same thing.
XOVI_REPO_URL=https://github.com/asivery/xovi.git
XOVI_COMMIT=2b99649f5e4fd6288be7792a8570bd16418adb70
XOVI_EXT_URL=https://github.com/asivery/rm-xovi-extensions/releases/download/v19-23052026/xovi-aarch64.tar.gz
XOVI_EXT_SHA=32d64d1262ddc984e3235c7d0340a398fe6d5b3efa6a979865f5977b32630d27
RMSHOT_URL=https://github.com/rmitchellscott/rm-shot/releases/download/v1.2.0/rm-shot-aarch64.so
RMSHOT_SHA=1526fb4a0582c26801afc2861eeafd651286ef0f23bbdb372700dd025f66f786

say() { printf '\n==> %s\n' "$*"; }
# Progress bar on a terminal; one line per file in CI logs.
if [ -t 2 ]; then WGET=(wget -q --show-progress); else WGET=(wget -nv); fi
need() { command -v "$1" >/dev/null 2>&1; }
git_here() { git -C "$here" -c safe.directory="$here" "$@"; }   # Windows-drive checkouts can look "dubious" to WSL git
fetch() { # fetch URL DEST SHA256
    if [ -f "$2" ] && echo "$3  $2" | sha256sum -c --quiet - >/dev/null 2>&1; then echo "  $(basename "$2"): already downloaded"; return; fi
    "${WGET[@]}" -O "$2.part" "$1"
    mv "$2.part" "$2"
    echo "$3  $2" | sha256sum -c --quiet - || { echo "Checksum mismatch for $2; delete it and retry, or check the pinned version."; exit 1; }
}
is_arch() { local kind; kind=$(file -b "$1"); [[ $kind == *"$EXPECT_ARCH"* ]]; }

git_here rev-parse --is-inside-work-tree >/dev/null 2>&1 || { echo "Run this from a git checkout of the repository ($here is not one)."; exit 1; }
mkdir -p "$WORK"

if [ "${1:-}" != --bundle ]; then
    [ "$(uname -s)" = Linux ] || { echo 'Run this on Linux or in WSL (Ubuntu).'; exit 1; }
    [ "$(uname -m)" = x86_64 ] || echo "Note: the reMarkable SDK installer is built for x86_64 hosts; this is $(uname -m)."

    say 'Build tools'
    if need apt-get; then
        missing=()
        for p in build-essential python3 git wget file xz-utils; do dpkg -s "$p" >/dev/null 2>&1 || missing+=("$p"); done
        if [ ${#missing[@]} -gt 0 ]; then
            echo "  installing: ${missing[*]} (sudo may ask for your Linux password)"
            sudo apt-get update -qq
            sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq "${missing[@]}" >/dev/null
        else echo '  already installed'; fi
    else
        for t in g++ make python3 git wget file xz; do need "$t" || { echo "Install $t first"; exit 1; }; done
    fi

    say "reMarkable SDK for the Paper Pro (ferrari), OS $OS_VERSION"
    if ! compgen -G "$SDK_DIR/environment-setup-*" >/dev/null; then
        mkdir -p "$(dirname "$SDK_INSTALLER")"
        if [ ! -f "$SDK_INSTALLER" ]; then
            echo "  downloading $SDK_URL"
            "${WGET[@]}" -O "$SDK_INSTALLER.part" "$SDK_URL"
            mv "$SDK_INSTALLER.part" "$SDK_INSTALLER"
        fi
        chmod +x "$SDK_INSTALLER"
        "$SDK_INSTALLER" -y -d "$SDK_DIR"
    else echo "  already installed in $SDK_DIR"; fi
    envs=("$SDK_DIR"/environment-setup-*)
    SDK_ENV=${envs[0]}
    echo "  environment: $SDK_ENV"

    say "xovi tooling (xovigen.py, pinned at ${XOVI_COMMIT:0:12})"
    if [ ! -d "$WORK/xovi/.git" ]; then git clone -q "$XOVI_REPO_URL" "$WORK/xovi"; fi
    git -C "$WORK/xovi" cat-file -e "$XOVI_COMMIT^{commit}" 2>/dev/null || git -C "$WORK/xovi" fetch -q origin
    git -C "$WORK/xovi" -c advice.detachedHead=false checkout -q "$XOVI_COMMIT"

    say 'Compiling (on a copy of the tracked files)'
    rm -rf "$WORK/src" && mkdir -p "$WORK/src"
    git_here ls-files -z | (cd "$here" && tar --null -cf - -T -) | tar -C "$WORK/src" -xf -
    rm -rf "$WORK/src/bin" "$WORK/src/build"
    ( cd "$WORK/src" && SDK_ENV="$SDK_ENV" XOVI_REPO="$WORK/xovi" XOVI_ARCH=aarch64 JOBS="$(nproc)" bash build.sh )
    for b in "$WORK/src/bin/ink-diary.so" "$WORK/src/bin/config-model"; do
        is_arch "$b" || { echo "$b is not an $EXPECT_ARCH binary ($(file -b "$b")); the SDK environment was not applied."; exit 1; }
    done
    echo "  built: $(file -b "$WORK/src/bin/ink-diary.so" | cut -d, -f1-2)"
else
    for b in "$WORK/src/bin/ink-diary.so" "$WORK/src/bin/config-model"; do
        [ -f "$b" ] && is_arch "$b" || { echo "No previous Paper Pro build in $WORK/src/bin; run without --bundle first."; exit 1; }
    done
fi

say 'Helper extensions for tablets without Vellum (checksum-pinned)'
mkdir -p "$WORK/deps"
fetch "$XOVI_EXT_URL" "$WORK/deps/xovi-aarch64.tar.gz" "$XOVI_EXT_SHA"
fetch "$RMSHOT_URL" "$WORK/deps/rm-shot-aarch64.so" "$RMSHOT_SHA"

say 'Bundle'
stage="$WORK/stage"
rm -rf "$stage" && mkdir -p "$stage"
# Tracked files from this checkout, the fresh aarch64 binaries, and the helper extensions.
git_here ls-files -z | (cd "$here" && tar --null -cf - -T -) | tar -C "$stage" -xf -
rm -rf "$stage/.github" "$stage/.gitattributes"
cp "$WORK/src/bin/ink-diary.so" "$WORK/src/bin/config-model" "$stage/bin/"
mkdir -p "$stage/deps" && cp "$WORK/deps/xovi-aarch64.tar.gz" "$WORK/deps/rm-shot-aarch64.so" "$stage/deps/"
# Checksums of exactly what ships, so the tablet-side `sha256sum -c` and the archive always agree.
( cd "$stage" && find . -type f ! -name SHA256SUMS | sed 's|^\./||' | LC_ALL=C sort | xargs -d '\n' sha256sum > SHA256SUMS )
mkdir -p "$here/dist"
bundle="$here/dist/ink-diary-paperpro.tar.gz"
tar -C "$stage" -czf "$bundle.part" --owner=0 --group=0 --mode=u+rwX,go+rX,go-w .
mv "$bundle.part" "$bundle"
echo "  $bundle ($(du -h "$bundle" | cut -f1), $(wc -l < "$stage/SHA256SUMS") files)"
printf '\nDone. Next: run deploy-paperpro.ps1 in Windows PowerShell with the tablet on USB,\nor follow INSTALL-PAPER-PRO.md by hand.\n'
