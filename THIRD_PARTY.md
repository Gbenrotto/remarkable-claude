# Attribution

- `src/inserter.inc` derives from [Zen-Ink/xovi-content-inserter](https://github.com/Zen-Ink/xovi-content-inserter), commit `309519ddeaa1b279fd60fcfec31cf89e553df844`, copyright © 2026 NFJ / Nuanfengjia, GPL-3.0-only. Changes: removed constructor diagnostics, limited exported API, added cancellation/page identity guard before each ink run. Original upstream native insertion code remains in the source but is not exported by this plugin.
- The DocumentView QMD selector comes from [rmitchellscott/xovi-qmd-extensions](https://github.com/rmitchellscott/xovi-qmd-extensions), `3.27/penLayerMemory.qmd`, copyright Mitchell Scott, GPL-3.0-only. The inserted control is new.
- `fonts/Caveat.ttf` comes from [Google Fonts / Caveat](https://github.com/google/fonts/tree/main/ofl/caveat), under SIL Open Font License 1.1; see `fonts/OFL.txt`.
- XOVI generated glue is produced by [asivery/xovi](https://github.com/asivery/xovi) commit `2b99649f5e4fd6288be7792a8570bd16418adb70`.
- Runtime dependency [rm-shot](https://github.com/rmitchellscott/rm-shot), GPL-3.0, installed separately through Vellum.
- The Paper Pro bundle (`dist/ink-diary-paperpro.tar.gz` from `build-paperpro.sh` or the GitHub Actions artifact) carries unmodified upstream binaries in `deps/` for tablets without Vellum. Their complete corresponding source is published by their authors at the pinned versions, and `build-paperpro.sh` verifies each download against a SHA-256 checksum:
  - `deps/xovi-aarch64.tar.gz`: [asivery/rm-xovi-extensions](https://github.com/asivery/rm-xovi-extensions) release `v19-23052026` (source: [tag v19-23052026](https://github.com/asivery/rm-xovi-extensions/tree/v19-23052026)), GPL-3.0. install.sh uses `xovi-message-broker`, `framebuffer-spy`, `qt-resource-rebuilder` and `rebuild_hashtable` from it.
  - `deps/rm-shot-aarch64.so`: [rmitchellscott/rm-shot](https://github.com/rmitchellscott/rm-shot) release `v1.2.0` (source: [tag v1.2.0](https://github.com/rmitchellscott/rm-shot/tree/v1.2.0)), GPL-3.0.

New code in this project is GPL-3.0-only. NOTICE retains the upstream copyright and attribution notice; LICENSE is the full, unmodified GNU GPL version 3 text.

- Button icon (`icons/claude.svg`): a generic asterisk drawn for this fork, not a brand logo. The upstream OpenAI icon (Simple Icons, CC0-1.0) was removed.

- DejaVu Sans Mono: bundled for code rendering under the Bitstream Vera font license; see `fonts/DejaVu-LICENSE.txt`.
- The Claude API port this Paper Pro branch builds on is [jonaslaberg/remarkable-claude](https://github.com/jonaslaberg/remarkable-claude), GPL-3.0-only; the Paper Pro changes are listed in CHANGELOG.md.
