# Ink Diary for Claude — reMarkable 2

A fork of [mmoscat/remarkable-chatgpt](https://github.com/mmoscat/remarkable-chatgpt) (Ink Diary 4.1) that talks to Anthropic's Claude instead of OpenAI.

**Installation and configuration: [INSTALL.md](INSTALL.md).**

A Claude button inside the standard notebook app. Tap it to send the visible notebook area to the Claude API over the tablet's Wi-Fi, render the answer in Caveat handwriting, and insert it beneath your writing as a single handwriting image. No companion computer is needed after setup.

Target: **reMarkable 2**, any OS version whose Qt is binary compatible with the SDK used for the build. The checked-in `bin/ink-diary.so` is built with the **3.28.0.172** SDK (Qt 6.10.3). This is a development prototype.

**Port status:** built with the 3.28.0.172 SDK, host unit tests pass, and live web search has been verified against the Claude API. It has not yet been run on a tablet. See VALIDATION.md.

### Changes from upstream

- Requests go to `https://api.anthropic.com/v1/messages` (`x-api-key`, `anthropic-version: 2023-06-01`). Notebook reading uses Claude vision plus structured outputs (`output_config.format`) with the same answer schema as upstream.
- Web search uses Anthropic's server-side `web_search` tool. Citations come from the API's `web_search_result_location` metadata.
- Email: triage, search and handwritten replies with a Send button (Gmail, app password).
- AI image generation (comics, illustrations) was removed because the Claude API has no image-generation endpoint. Such requests get a short text reply suggesting a diagram instead. Plots, Mermaid flowcharts, sequence diagrams, Venn diagrams, checklists, tables and calendars still render locally on the tablet.
- Default model `claude-sonnet-5-5`. On Sonnet 5.5, Opus 5.5, Opus 5 and Fable 5.1, requests opt into server-side refusal fallbacks (`fallbacks: "default"`).

## Demo mode

For offline testing, config.json may contain `{"demo_mode":true}`. Holding the icon shows **Claude (demo)** and inserts a fixed handwriting sample below your writing without making any API request. Running `configure.sh` replaces demo mode with the real API configuration.

## First use

1. Configure your own Anthropic API key privately in the reManager terminal:
   ```sh
   sh /home/root/.config/ink-diary/configure.sh
   ```
   The key entry is hidden. The default model is `claude-sonnet-5-5`; any Claude model with vision and structured outputs can be entered (for example `claude-opus-5-5` or `claude-haiku-5-5`). API usage is billed to your Anthropic API account. No Claude.ai conversation is created.
2. Enable Wi-Fi. Open a disposable notebook with a **blank or lightly ruled template**, in **portrait orientation**, at normal zoom.
3. The answer image uses black handwriting independently of the selected pen.
4. Write a short question anywhere in the visible capture area, including at the top, away from the left toolbar. Leave the lower half clear.
5. Tap the **Claude icon** at the top right, under the toolbar's close button. Keep the page still and lift your pen until it finishes. While working, the icon becomes a stop square; tap it to cancel. Completion and error messages appear briefly above the button. Hold the icon to see its current status.
6. The answer is laid out in the Caveat handwriting font and inserted as one transparent image. Letters are not editable pen strokes. The native notebook app controls image sizing and selection.

## Web search

Write `Search` followed by a question to explicitly request a web lookup, for example `Search what is the screen resolution of reMarkable 2?`. Questions about current facts, news, weather, prices and schedules are also routed to search automatically by the model. Include a location when needed. Ordinary timeless questions and function plots do not normally need a lookup.

The first request reads the page and routes the question. Search requests then make a second Messages API call with Anthropic's server-side `web_search` tool (`web_search_20250305`, at most three searches). Claude Opus 5.5 and Sonnet 5.5 do not accept a forced `tool_choice`, so the system prompt tells Claude to search. The plugin rejects an answer unless a search actually returned results. A search error (for example `too_many_requests`) is shown as a status message. Long searches that return `pause_turn` are resumed automatically up to three times.

Answers include numbered citations and a source list built from the API's `web_search_result_location` citations (URL and title). Links are never invented. If the search ran but Claude cited nothing, the answer is shown with an explicit source-links-unavailable notice. Sources use a smaller print font for legibility. Notebook PNGs are not clickable. Each searched answer also has a sibling `.png.html` file in `/home/root/.local/share/ink-diary/answers/` with clickable citations; copy it through reManager to read in a browser. HTML text and links are escaped.

Search requests use your local timezone (default `America/New_York`, set with the optional `timezone` field), with automatic daylight-saving conversion. A tablet-generated "Checked at" line records when the response arrived; it is not the weather observation time.

Search adds latency and costs $10 per 1,000 searches plus tokens. Web search can be disabled for your organization in the Claude Console; requests then fail with an HTTP 400 status message. Demo mode makes no API requests.

## Email

Write `check my email` to get one numbered, handwritten item per email that needs you (newsletters and notifications are counted, not listed), `find emails from Anna` to search with Gmail syntax, and `reply 2: …` (or a line from a summary to your note) to have Claude draft reply-all emails on the page. A **Send N** button appears while drafts are pending; one tap sends them all, threaded. Checks mark what they looked at as read; searches don't. Setup and details: INSTALL.md, section 8. Gmail is reached directly from the tablet over IMAP/SMTP with an app password stored only on the tablet; email text goes to the Claude API for summarising and drafting.

## Plot commands

Write `plot` followed by one equation, then tap the Claude icon. Examples:

- `plot y = x^2`
- `plot y = sin(x)`
- `plot y = 2x + 1 from x = -10 to 10`

Handwritten superscripts are accepted through image recognition. The model transcribes the equation; a restricted on-tablet math parser computes the curve. No generated code is executed and no image-generation service is used. The interpreted equation appears above the graph so you can check recognition.

The default x range is -5 to 5; y is automatically scaled from sampled values. Axes have independent scales. Trigonometric functions use radians; ln/log mean natural logarithm. Supported operations: +, -, multiplication, division, powers, parentheses, sin, cos, tan, sqrt, abs, exp, ln, log, pi and e. Only explicit single-variable functions y=f(x) are supported, not implicit equations, multiple curves or parametric plots. Very narrow features can be missed by sampling; undefined values and detected asymptotes break the curve.

Answers and plots append below existing page content, extending beyond the visible screen when necessary. Scroll down to see the rest. Graphs use a white background for legibility over ruled templates and are inserted as image objects. Ordinary questions retain handwriting-image answers. Demo mode remains the original fixed text sample. The configured model must support structured outputs (all current Claude models do).

## Generated images and diagrams

The Claude API cannot generate images, so upstream's artwork feature (comics, illustrations) is not available. Such requests get a short text reply, often suggesting a diagram instead.

Diagrams are drawn locally with 3–4 px black lines and print labels. Examples:

- `Create a Mermaid flowchart for making tea.`
- `Create a sequence diagram of a tablet sending a question to Claude.`
- `Create a Venn diagram comparing cats and dogs.`

The model produces a restricted Mermaid subset: flowcharts using TD/TB source syntax (up to 12 nodes and 24 arrows), laid out in a three-column wrapping grid, or sequence diagrams (2–5 participants and up to 20 messages). This is a native subset renderer, not the complete Mermaid JavaScript engine. Class, ER, Gantt, state diagrams, arbitrary Mermaid directives and mixed Markdown/diagram documents are not supported. Flowcharts support rectangular, rounded and diamond nodes, arrows and short edge labels. Complex branching/loop layouts may need a simpler request.

Venn diagrams support two or three sets with numbered exclusive regions and a readable content legend underneath. Circle areas are not proportional to counts. Diagram source is saved privately beside answer PNGs as `.png.source.txt`; ordinary text keeps `.png.md`.

## Prototype limits

- Portrait orientation and short answers only. Light gray full-width horizontal rules and thin vertical margin lines (including those partly hidden by plots) are filtered out when locating handwriting. Dark or complex templates may still be treated as ink.
- The working area is screen pixels x=150..1323, y=0..1751. Keep questions there. The side and bottom UI margins are cropped out of the uploaded screenshot; the top edge is included.
- Native image insertion may extend the notebook page below the viewport. Scroll down to follow it; there is no automatic scrolling or new-page creation. The visible question still needs to be inside the capture area. Answers have a fixed image-size limit, independent of remaining screen space.
- Visible page content supplies conversation context; there is no separate persistent conversation history.
- Page identity and a second screenshot are checked before insertion. Keep the page still until completion. Native image insertion is asynchronous; cancellation cannot retract an insertion already handed to the notebook app.
- Font coverage limits output languages and symbols. Unsupported characters produce an error rather than missing glyphs.
- Native image insertion and screen-to-page coordinate mapping must be supported by the installed firmware.
- Firmware updates require revalidation. Instead of a fixed version, the extension declines to load if the firmware's Qt is older than, or a different major version from, the Qt it was built against. Notebook-app internals (page view, native image insertion, screen size) are probed at runtime, and a missing one produces a status message. A firmware update can still change those internals: rebuild with the matching SDK, and remove the QMD patch if the UI misbehaves.

## Installation

Follow **[INSTALL.md](INSTALL.md)**: SSH access, Vellum and xovi, copying this repo, `install.sh`, the required `xovi/rebuild_hashtable` step, your API key, and `xovi/start`. xovi is tethered, so the button is gone after a reboot: triple-press the power button (with the optional xovi-tripletap starter from INSTALL.md), or run `bash /home/root/xovi/start` over SSH.

## Removal

```sh
sh /home/root/ink-diary-claude/uninstall.sh
```

Restart the UI afterwards. Your notebooks and existing Vellum packages remain. The API key/configuration remains at `/home/root/.config/ink-diary/config.json`; remove that specific file if you want to delete the saved credential.

## Privacy and network behavior

Only tapping Claude starts a request. The cropped visible notebook image goes to `https://api.anthropic.com/v1/messages` over HTTPS with certificate verification. Anthropic's API data-retention policy applies; see https://platform.claude.com/docs/en/manage-claude/api-and-data-retention . Web searches send a standalone rewritten question in a second request. Screenshots use a private temporary directory and are deleted on normal completion/error. Answer PNGs remain in the root-only directory `/home/root/.local/share/ink-diary/answers` because the notebook app imports them asynchronously. An app crash may leave temporary files until reboot. Diagram rendering is local after the reading request. Notebook content and credentials are not printed to logs. The API key is saved locally with root-only permissions.

## Build and test

Build with the official rM2 SDK matching the tablet's OS version, listed at https://developer.remarkable.com/links . The checked-in binary uses 3.28.0.172:
https://storage.googleapis.com/remarkable-codex-toolchain/3.28.0.172/remarkable-production-image-5.8.203-rm2-public-aarch64-toolchain.sh (an x86_64 variant is also published).

The SDK runs on Linux. On a Mac, use a Linux VM (for example Lima, `limactl start template:ubuntu-lts`), and build from a copy on the VM's own disk: macOS filesystems are case-insensitive, and the repo's `VERSION` file then shadows the C++ `<version>` header.

```sh
export SDK_ENV=/absolute/path/to/sdk/environment-setup-cortexa7hf-neon-remarkable-linux-gnueabi
export XOVI_REPO=/absolute/path/to/xovi
./build.sh
```

Output: `bin/ink-diary.so` (ARMv7). Host checks require a C++ compiler and Qt6 Core/Gui development packages:

```sh
./test.sh
```

The checks cover placement boundaries, Messages API JSON handling (refusal, max_tokens, fallback, web search citations, search errors, pause_turn), and actual font rendering. They do not test live network requests or physical pen injection.

Upstream provenance and licenses: THIRD_PARTY.md. API documentation: https://platform.claude.com/docs

Append-placement regression checks can also be run with `node tests/append.cjs`.

## Markdown answers

Answers now render GitHub-style Markdown using the tablet's Qt text engine: six heading levels, bold/italic/strikethrough, ordered and unordered nested lists, task checkboxes, tables with column alignment, inline and fenced/indented code, blockquotes, links, horizontal rules, and Markdown escapes. Paragraphs and headings use Caveat handwriting; tables and code use readable print/monospaced fonts. Code is displayed, not executed, and has no syntax highlighting. Links and checkboxes in the notebook are static ink images; links work in the saved sibling HTML file.

Text answers can grow to 8,000 pixels high and append below existing content. Very large answers or tables are rejected with a request to shorten them. Lines wrap, including long code lines. The model is asked for concise answers and tables with at most five columns. Citations, source notices, and local checked-at times remain visible.

Markdown dialects differ: raw HTML, LaTeX, Mermaid, footnotes, and embedded remote images are not supported. Image markup becomes an alt-text placeholder; it never downloads an image or reads a local file. Function plots still use the existing `plot` command. Formatting changes apply to new answers; existing notebook images stay as they are.

A synthetic example is available in `markdown-preview.png`. The archived version 2.0 is unchanged.

### Current dates and recent facts

Both the notebook-reading stage and web-search stage receive the tablet's current local date, time, and timezone. The reading stage is instructed to treat this as the present even when it is later than the model's training cutoff, and to disregard stale prior notebook answers. A fallback checks the transcribed latest question for English freshness phrases and current-officeholder questions and forces the search route if the model incorrectly labels it ordinary text. Other languages still rely on model routing. This does not guarantee source accuracy; search is instructed to admit when current facts cannot be verified. The tablet clock must remain correct. Prefixing a question with `Search` remains available for explicit web lookup.

### Change only the model

In reManager's terminal, run:

```sh
sh /home/root/.config/ink-diary/configure.sh --model claude-opus-5-5
```

The `--model` option preserves the saved API key and every other configuration field. It validates the name and existing JSON, then saves atomically with owner-only permissions. No restart is needed; the next question uses the new model, for both notebook reading and web search. Use a Claude model with vision and structured outputs that your API account can access, for example `claude-sonnet-5-5` (default), `claude-opus-5-5` or `claude-haiku-5-5`. Costs and latency differ. Running the helper without `--model` repeats the full key and model setup.

For faster or cheaper answers without changing model, add `"effort":"low"` (or `medium`, `high`, `xhigh`, `max`) to config.json.

The prompts say the answer is rendered as GitHub Markdown. Tables and templates must be emitted outside fences; fences are reserved for programming code or explicitly requested raw Markdown source.

Blank checklists are supported: request `create a 10 item checklist with checkboxes without labels`. Each empty task item gets an invisible layout placeholder so the checkbox keeps its own writable row. Text answers also keep the model's original Markdown in a private sibling `.png.md` file. The diagnostic message `ink-diary$probe` with argument `checklist` tests ten synthetic blank rows inside the notebook process without inserting anything.

## Structured templates

Checklist and single-table creation requests now use dedicated response kinds in the existing API call. The model supplies a count and optional labels for checklists, or columns and rows for tables. The plugin generates blank rows itself, escapes cell/label text, validates counts and dimensions, and renders the result. Blank templates cannot collapse because of empty model-generated Markdown. Supported limits are 1–50 items/body rows and up to 8 table columns; image size limits still apply. Mixed explanatory answers and web-search answers continue to use Markdown. No additional classifier request is needed.

Examples: `Create a 10 item checklist without labels`; `Create a pros and cons table with 6 blank rows`. With no number specified, the prompts default to ten checklist items or five blank table rows. Handwriting recognition/classification can still make mistakes.

Checkbox outlines, table borders, horizontal dividers, quote bars and plot frames/axes use 3-pixel strokes at source-image resolution; plot gridlines use 2 pixels. Checkbox outlines are painted explicitly for consistency. These changes apply to new answers. Saved `.png.md` files contain model Markdown for ordinary answers and locally generated Markdown for structured templates.

### Writable calendar/table cells

Table templates now carry `cell_lines` (1–10). `1` keeps the compact layout; each additional handwriting line reserves 60 source-image pixels beneath the cell's text. Header rows stay compact. For example: `Create a calendar for November 2026 with each day cell 3 rows tall so I can write in it`. For monthly calendars the model identifies the month, year and requested height; the tablet computes the Gregorian dates and weekday positions using QDate. Sunday is the default first column; ask for Monday-start if preferred. Calendar cells default to three writing lines. HTML `<br>` tags are not used for spacing. This also works for other tables with requested writing space. Large layouts may extend down the notebook page or reach the existing image-height limit.

Calendar requests now default to three handwriting lines per day cell, even when you do not specify a height. An explicit requested height takes precedence. Ordinary tables retain their compact one-line default. This change is newer than the frozen version 3.0 snapshot.
