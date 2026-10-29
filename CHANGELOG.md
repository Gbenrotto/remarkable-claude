# Changelog

Changes in this fork relative to [mmoscat/remarkable-chatgpt](https://github.com/mmoscat/remarkable-chatgpt) (Ink Diary 4.1).

## Paper Pro port (branch paperpro, 2026-10-29)

- Screen geometry follows the captured screen size: reMarkable 2 (1404 × 1872) and Paper Pro (1620 × 2160), portrait. Toolbar, button cells and margins keep their pixel sizes; the page area and answer budget grow with the screen (`src/layout.h`).
- The ink search leaves the masked button cells out of its template statistics, so a ruled template no longer counts as ink where the buttons cut a rule (affects both tablets).
- `diary_probe` reports the detected device, capture area and button rectangles; `diary_send` is now exported in the version script like the other entry points.
- Timezones accept fixed offsets such as `+09:30` for tablets without zone data; `configure.sh` asks for the timezone, validates it, and offers `--timezone`.
- `install.sh` accepts a Paper Pro (aarch64, "ferrari"), refuses binaries built for the other tablet, and installs the helper extensions from the bundle when Vellum is absent (remagic setups), stopping on any failed copy.
- `build-paperpro.sh` (Linux/WSL) compiles against the ferrari SDK with pinned xovi tooling and checksum-pinned helper extensions, staging everything outside the checkout; the bundle carries its own SHA256SUMS of exactly what ships.
- `deploy-paperpro.ps1` deploys from Windows over USB, from a local build or the GitHub Actions artifact, configuring only on first install so Gmail and other settings survive redeploys.
- GitHub Actions workflow builds the bundle and runs the host tests; `tests/paperpro.cpp` simulates a tap on both tablets; `.gitattributes` keeps LF line endings on every platform.

## 2026-10-25

### Claude instead of OpenAI
- Notebook reading uses the Claude Messages API (`/v1/messages`) with vision and structured outputs. The answer schema is the same as upstream.
- Web search uses Anthropic's server-side `web_search` tool. Citations become numbered `[n]` markers with a source list. Search errors, `pause_turn`, refusals and `max_tokens` are handled. API error text is shown in the status message.
- Default model `claude-sonnet-5-5`, changeable with `configure.sh --model`, plus an optional `effort` setting. Server-side refusal fallbacks are on for models that support them.
- AI image generation was removed, because the Claude API has no image endpoint. Plots, Mermaid flowcharts and sequence diagrams, Venn diagrams, checklists, tables and calendars still render on the tablet.

### Better answers
- Terse by default: the answer comes first, about 60 words, or 80 for web answers, with caveats only when they matter.
- Circles, underlines and lines drawn from text to a note scope the note to exactly the marked words.

### Email (Gmail)
- `check my email`: one numbered, handwritten item per email that needs you, plus a skipped count. Everything checked is marked read.
- `find emails from …` / `the email about …`: Gmail-syntax search, up to 10 matches summarised, never changing read state. `show me …` prints one email in full.
- Reply on paper by number (`reply 2: …`), by a line from a summary to your note, or with a note under a summary. Claude writes reply-all drafts showing To/Cc and which email each answers. `2: shorter` revises one draft; `2: skip` drops it.
- A **Send N** button appears while drafts are pending. It first looks at the page: corrections drawn by hand on a printed draft (crossed-out words, insertions, replacements) are applied and sent in the same tap; the status names the edited drafts. All drafts go out threaded over Gmail SMTP.
- Gmail access uses an app password stored only on the tablet. `configure.sh --gmail` sets it up. Email text is treated as data, never as instructions.

### Firmware and hardware
- The exact OS 3.27.3.0 pin was replaced by a Qt ABI check at load (same major version, runtime minor ≥ build minor). Notebook-app internals are probed at runtime.
- Built and tested on OS 3.28.0.172 (Qt 6.10.3) with the official SDK. The installer warns, rather than refuses, on untested versions.
- The Claude button is a framed icon under the toolbar's close button, masked out of what Claude reads.
- Optional `xovi-tripletap`: triple-press the power button to bring the plugin back after a reboot, without a computer.

### Build, tests, docs
- Host tests pass on macOS (absolute font paths, pinned locale) and Linux. Live captured search fixture.
- Builds map the home folder out of binaries (`-ffile-prefix-map`).
- `INSTALL.md` rewritten from a real 3.28 install: SSH key, Vellum and xovi, the required `xovi/rebuild_hashtable` step, API key, Gmail setup, and starting after reboot.
