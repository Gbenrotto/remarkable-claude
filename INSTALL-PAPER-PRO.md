# Ink Diary for Claude on the reMarkable Paper Pro

This branch ports Ink Diary, written for the reMarkable 2, to the reMarkable Paper Pro. The upstream code assumed a 32-bit processor and a 1404 × 1872 screen. This port builds for the Paper Pro's 64-bit processor and 1620 × 2160 screen, keeps the same button position and page margins (both tablets run the same notebook interface at almost the same pixel density), and installs its helper extensions even on tablets set up with remagic rather than Vellum.

**Status:** the port passes the host test-suite, including a simulated Paper Pro tap from screenshot to insertion coordinates. It has not yet run on a real tablet, so the first on-device run is the remaining test. Everything installs under `/home/root`, and a reboot (or `xovi/stock`) returns the tablet to standard behaviour.

## What you need

- A reMarkable Paper Pro (not the Paper Pro Move) in developer mode, with xovi installed through remagic or Vellum. This build targets **OS 3.28.0.172**; check yours under Settings → General → Help → About.
- The USB cable. Over USB the tablet answers SSH at `root@10.11.99.1`.
- A Windows, macOS or Linux computer with `ssh` and `scp`. The deploy script is for Windows PowerShell; on macOS or Linux, follow "Deploy by hand".
- Somewhere to compile the 64-bit plugin: **GitHub Actions** (nothing to install) or **Linux / WSL**. reMarkable only publishes its SDK for Linux; the first build downloads about 1 GB.
- An Anthropic API key with credits (Claude Console → Billing, then API Keys). A Claude.ai subscription does not include API access, and each answer is billed to that key.

Developer mode lowers the tablet's security and the installer runs as root, so keep work-managed documents off a tablet set up this way.

## 1. Build (once per OS version)

**Option A — GitHub Actions (recommended on Windows).**

1. Fork this repository and open the fork's **Actions** tab. GitHub disables workflows on new forks, so click **I understand my workflows, go ahead and enable them**.
2. Open **Build for reMarkable Paper Pro** and click **Run workflow** on the `paperpro` branch (any push to `paperpro` also starts it). The first run takes about 15 minutes; later runs reuse the cached SDK installer.
3. When the run is green, download the **ink-diary-paperpro** artifact (a zip) from the bottom of the run page. Keep it zipped.

**Option B — Linux or WSL.** From a checkout of this branch:

```sh
bash build-paperpro.sh
```

It installs any missing compiler packages (asking for your password once), downloads and installs the Paper Pro SDK into `~/rm-sdk`, compiles and stages everything under `~/ink-diary-build` on the Linux filesystem, downloads the two helper-extension packages with checksum verification, and writes `dist/ink-diary-paperpro.tar.gz`. Nothing tracked in the checkout changes, so the reMarkable 2 binaries in `bin/` stay as they are.

On Windows without WSL: in an administrator PowerShell run `wsl --install -d Ubuntu`, reboot, create the Linux user when Ubuntu opens, then run the command above from the checkout (for example `bash /mnt/c/Users/<you>/src/remarkable-claude/build-paperpro.sh`).

## 2. Deploy (tablet on USB)

In PowerShell, from the checkout:

```powershell
# with the artifact from Option A
powershell -ExecutionPolicy Bypass -File .\deploy-paperpro.ps1 -Bundle "$env:USERPROFILE\Downloads\ink-diary-paperpro.zip"
# or with dist\ink-diary-paperpro.tar.gz from Option B
powershell -ExecutionPolicy Bypass -File .\deploy-paperpro.ps1
```

Run it from an ordinary PowerShell window rather than the ISE. It checks that the tablet is a 64-bit reMarkable with xovi, copies the bundle, verifies every checksum on the tablet (and installs nothing if one fails), runs `install.sh`, then goes through three interactive steps:

1. **Configure** (first install only): paste your API key at the hidden prompt, press Enter for the default model `claude-sonnet-5-5`, and type your timezone, for example `Australia/Darwin`. If the tablet has no zone data for it, the script asks for a fixed offset such as `+09:30` instead; that is exact for any zone without daylight saving.
2. **Hashtable:** `xovi/rebuild_hashtable` maps the notebook app's internal names so the button can be placed. Press Enter when asked; the UI restarts once. It is needed once per OS version; add `-SkipHashtable` on later deploys.
3. **Start:** `xovi/start` restarts the notebook app with the plugin, and the script waits up to 30 seconds to confirm that `ink-diary.so` is loaded.

Redeploying keeps your configuration (key, model, timezone, Gmail). Add `-Reconfigure` to enter it again.

### Deploy by hand (macOS, Linux, or no PowerShell)

```sh
# (unzip a GitHub artifact first to get ink-diary-paperpro.tar.gz)
scp dist/ink-diary-paperpro.tar.gz root@10.11.99.1:/home/root/
ssh root@10.11.99.1
rm -rf /home/root/ink-diary-claude && mkdir -p /home/root/ink-diary-claude
tar -xzf /home/root/ink-diary-paperpro.tar.gz -C /home/root/ink-diary-claude && cd /home/root/ink-diary-claude
sha256sum -c SHA256SUMS | grep -v ': OK$'      # prints nothing when every file verifies
sh install.sh
sh /home/root/.config/ink-diary/configure.sh    # first install only
bash /home/root/xovi/rebuild_hashtable          # once per OS version
bash /home/root/xovi/start
```

## 3. First tap

1. Create a **new notebook on the Paper Pro** (a notebook synced from a reMarkable 2 keeps rM2 page dimensions and is shown scaled). Use portrait, normal zoom, and a blank or lightly ruled template.
2. Write a short question near the top, away from the left toolbar: `Why does the Moon shine?`
3. Tap the Claude button (top right, under the toolbar's close button) and keep the page still. Hold the button to read its status; an error stays visible until the next tap.
4. The answer appears below your writing as one image. Scroll down if it runs past the screen.

Then try `plot y = x^2`, `Create a flow diagram of making tea`, and `Search the weather in Alice Springs right now`.

## Troubleshooting

### The button is missing or in the wrong place

The Paper Pro geometry is the one part that could not be verified without a tablet. Ask the plugin what it sees (PowerShell shown; in a macOS or Linux shell write `\$probe` instead of `` `$probe ``):

```powershell
ssh root@10.11.99.1 "echo '>eink-diary`$probe:' > /run/xovi-mb; cat /run/xovi-mb-out"
```

It returns the `canvas` size, `device`, `supportedScreen`, and the capture and button rectangles it will use. On a Paper Pro, `canvas` should be 1620 × 2160 and `device` should read `reMarkable Paper Pro`.

For a misplaced button, take a screenshot with an open notebook and copy it off:

```powershell
ssh root@10.11.99.1 "echo '>etakeScreenshot:/home/root/ink-diary-shot.png' > /run/xovi-mb; cat /run/xovi-mb-out"
scp root@10.11.99.1:/home/root/ink-diary-shot.png .
```

The fix is two numbers in `inkDiary.qmd` (`anchors.topMargin` and `anchors.rightMargin`) plus the matching cell in `src/layout.h`, followed by a rebuild. Please open an issue with the probe output and the screenshot so the geometry can be corrected for everyone.

### The notebook app falls back to the standard UI after `xovi/start`

```powershell
ssh root@10.11.99.1 "journalctl -u xochitl -b --no-pager | grep -i -E 'ink-diary|qmldiff|xovi' | tail -40"
```

A Qt-version line means the build does not match the OS version. To check whether this plugin is the cause, move it aside and start again; if the fallback persists, another extension in `/home/root/xovi/extensions.d` is responsible:

```powershell
ssh root@10.11.99.1 "mv /home/root/xovi/extensions.d/ink-diary.so /home/root/ink-diary.so.off && bash /home/root/xovi/start"
```

### Other messages

See the troubleshooting section of [INSTALL.md](INSTALL.md); status messages, API errors and Gmail errors behave the same on both tablets.

## Day to day

- **After a reboot** the plugin is off, because xovi is tethered by design. Triple-press the power button if tripletap is installed (remagic sets it up), or run `ssh root@10.11.99.1 "bash /home/root/xovi/start"`.
- **After a reMarkable OS update**, take the new ferrari aarch64 toolchain link from [developer.remarkable.com/links](https://developer.remarkable.com/links), rebuild with `OS_VERSION=x.y.z.w SDK_URL=<link> bash build-paperpro.sh` (or update the version and cache key in the workflow), redeploy, and rerun the hashtable step.
- **Settings:** change the model with `configure.sh --model claude-haiku-5-5`, the timezone with `configure.sh --timezone +09:30`, and set up Gmail with `configure.sh --gmail` (see INSTALL.md section 8). Each keeps the other settings. The scripts live in `/home/root/.config/ink-diary/`.
- **Remove:** `ssh root@10.11.99.1 "sh /home/root/ink-diary-claude/uninstall.sh && bash /home/root/xovi/start"`. The helper extensions stay, since other extensions may use them. The API key stays in `/home/root/.config/ink-diary/config.json`; delete that file to remove it.

## What changed for the Paper Pro

| Area | reMarkable 2 (upstream) | Paper Pro port |
|---|---|---|
| Binary | ARMv7 32-bit, rm2 SDK | aarch64, ferrari SDK 3.28.0.172 (`build-paperpro.sh`, GitHub Actions) |
| Screen check | exactly 1404 × 1872 | 1404 × 1872 or 1620 × 2160 (`src/layout.h`) |
| Capture area | x 150–1323, y 0–1751 | x 150–1539, y 0–2039 (same toolbar and margins, larger page) |
| Button cells | top-right 112 px cell | same cell, anchored to the new right edge |
| Answer height budget | 1582 px | 1870 px |
| Ink search | a template rule cut short by the masked button cells counted as ink | masked cells left out of the template statistics (fixes both tablets) |
| Timezone | IANA names only | IANA names, or fixed offsets such as `+09:30` for tablets without zone data |
| Installer | Vellum only; refused anything but an rM2 | Vellum or bundled helper extensions; refuses a binary built for the other tablet |
| Tests | rM2 geometry | both geometries, plus a simulated tap end to end (`tests/paperpro.cpp`) |

Diagrams (flowchart, sequence, Venn) still render at the rM2 width of 1174 px, centred on the wider page; widening them is cosmetic and can follow once the port is confirmed on a device.
