#!/bin/sh
# Regenerate the committed SHA256SUMS (the reMarkable 2 release: every tracked file, with
# the rM2 binaries in bin/). Run after editing any shipped file, before committing.
# The Paper Pro bundle gets its own SHA256SUMS from build-paperpro.sh.
set -eu
cd -- "$(dirname -- "$0")"
here=$(pwd)
git -c safe.directory="$here" rev-parse --is-inside-work-tree >/dev/null 2>&1 || { echo 'make-sums.sh needs a git checkout'; exit 1; }
git -c safe.directory="$here" ls-files | grep -v -E '^(SHA256SUMS|\.github/.*|\.gitattributes)$' \
    | LC_ALL=C sort | xargs -d '\n' sha256sum > SHA256SUMS.new
mv SHA256SUMS.new SHA256SUMS
printf '%s files listed in SHA256SUMS\n' "$(wc -l < SHA256SUMS)"
