# Ink Diary for Claude — installation and configuration

This takes about 15 minutes. Steps 1–6 were run exactly as written on a reMarkable 2 with OS 3.28.0.172 on 2026-10-25.

## Requirements

- **reMarkable 2.** The checked-in plugin is built for OS 3.28.0.172 (Qt 6.10.3); check yours under Settings → General → Help → About. Other OS versions need a build with their matching SDK (see "Build and test" in README.md). The installer warns on untested versions, and the plugin refuses to load on an incompatible Qt.
- A computer with `ssh` and `git` (macOS or Linux), and a USB cable.
- Tablet Wi-Fi, for package downloads and API requests.
- An Anthropic API key with credits. In the [Claude Console](https://platform.claude.com), add credits under **Billing**, then use **API Keys → Create Key** and copy it (it starts with `sk-ant-` and is shown only once). A Claude.ai subscription does not include API access.

The plugin, xovi and Vellum all install under `/home/root`; the system partition is not modified. xovi is *tethered*: a reboot always returns the tablet to stock, and step 6 brings the plugin back. Notebooks are unaffected, but keep them synced or backed up anyway.

## 1. Enable SSH access

Connect the tablet over USB. The root password is at the bottom of **Settings → General → Help → About → Copyrights and licenses → GPLv3 Compliance**, along with the tablet's IP addresses (`10.11.99.1` over USB).

On your computer, install a key so you don't need the password again:

```sh
ssh-keygen -t ed25519 -f ~/.ssh/remarkable_ed25519 -N ""
ssh-copy-id -i ~/.ssh/remarkable_ed25519.pub root@10.11.99.1     # enter the tablet password once
printf 'Host remarkable\n  HostName 10.11.99.1\n  User root\n  IdentityFile ~/.ssh/remarkable_ed25519\n' >> ~/.ssh/config
ssh remarkable 'grep RELEASE_VERSION /usr/share/remarkable/update.conf'
```

The commands below use the `remarkable` host alias.

## 2. Install Vellum and xovi

[Vellum](https://github.com/vellum-dev/vellum-cli) is the package manager; [xovi](https://github.com/asivery/rm-xovi-extensions) loads extensions into the notebook app. On the tablet (`ssh remarkable`):

```sh
wget --no-check-certificate -O bootstrap.sh https://github.com/vellum-dev/vellum-cli/releases/latest/download/bootstrap.sh
echo "7b0deebc81b28a7d74d95c85e99a4a0a0f6ecaa5b9edb6b858ac61405978ebb9  bootstrap.sh" | sha256sum -c && bash bootstrap.sh
export PATH=/home/root/.vellum/bin:$PATH
vellum add xovi
```

The checksum is the one published in the Vellum README; if it doesn't match, the bootstrap script was updated, so take the current command from there. The bootstrap verifies everything it downloads.

## 3. Copy this repository to the tablet

On your computer:

```sh
git clone https://github.com/jonaslaberg/remarkable-claude ink-diary-claude
cd ink-diary-claude
git archive --format=tar HEAD | ssh remarkable 'rm -rf /home/root/ink-diary-claude && mkdir -p /home/root/ink-diary-claude && tar -xf - -C /home/root/ink-diary-claude'
```

## 4. Verify and install

On the tablet:

```sh
cd /home/root/ink-diary-claude
sha256sum -c SHA256SUMS | grep -v ': OK$'     # prints nothing when every file verifies
export PATH=/home/root/.vellum/bin:$PATH
sh install.sh
```

The installer checks the hardware and OS, installs xovi's extensions through Vellum (`qt-resource-rebuilder`, `xovi-message-broker`, `framebuffer-spy`, `rm-shot`), and copies the plugin, notebook patch, fonts, icon and configuration helper. It keeps an existing API configuration.

Then build xovi's hashtable, which maps the notebook app's internal names so the patch can find where to put the button. **This step is required; without it the button never appears.** Run it once after installing, and again after every OS update:

```sh
bash /home/root/xovi/rebuild_hashtable
```

It stops the notebook app and asks you to press Enter. The app then runs once on its own; if the tablet shows its passcode screen, enter your passcode. It ends by itself when it prints `Hashtable rebuilt`.

## 5. Configure your API key

```sh
sh /home/root/.config/ink-diary/configure.sh
```

Paste your API key at the hidden prompt, then press Enter at the model prompt for the default, `claude-sonnet-5-5`. Any Claude model that supports image input and structured outputs works, for example `claude-opus-5-5` (slower, more capable) or `claude-haiku-5-5` (fastest, cheapest). Web search also needs the web search tool, which an organization admin can disable in the Claude Console.

The key is stored only on your tablet in `/home/root/.config/ink-diary/config.json`, with owner-only permissions. Requests go directly from your tablet to Anthropic. Don't share that file.

To change only the model later, keeping your key and other settings: `sh /home/root/.config/ink-diary/configure.sh --model MODEL_NAME`. Settings are read on each tap, so no restart is needed.

Optional settings can be added to `config.json` (keep the JSON valid and run `chmod 600` on it afterwards):

- `timezone`: IANA timezone name, such as `Europe/London`. Default: `America/New_York`. Used for "today", "now" and the checked-at time on web answers.
- `effort`: `low`, `medium`, `high`, `xhigh` or `max`. Trades answer depth for speed and cost. Default: the model's own default (`high` on Sonnet 5.5, `medium` on Opus 5.5 and Haiku 5.5). `low` gives the snappiest answers.
- `demo_mode`: `true` for an offline fixed handwriting sample, `false` for normal API use. Demo mode does not test diagrams or calendars.

## 6. Start Ink Diary (and after every reboot)

```sh
bash /home/root/xovi/start
```

This restarts the notebook app with the extensions loaded; the screen returns to the library. Open a notebook in portrait orientation at normal zoom; the Claude button (an asterisk) appears at the top right, just under the toolbar's close button.

**After every reboot the button is gone**, because xovi deliberately doesn't start on boot. Don't make xovi start automatically by editing systemd files: xovi's README warns this can leave the tablet stuck in a boot loop. Instead, use one of these:

- **Without a computer (recommended):** install [xovi-tripletap](https://github.com/rmitchellscott/xovi-tripletap), the starter the xovi README recommends. Then **triple-press the power button** after a reboot to start xovi.
  ```sh
  export PATH=/home/root/.vellum/bin:$PATH
  vellum add tripletap
  ```
  It adds a small systemd service (`xovi-tripletap`) to the system partition that only listens for the power button; a plain reboot still comes up stock. Remove it with `vellum del tripletap`. After an OS update, run `vellum reenable` to restore it.
- **Over SSH:** run `bash /home/root/xovi/start` again.

## 5. Use it

Write in the visible page area, away from the Claude button in the top-right corner. The top edge is included. Tap the icon and keep the page still while it works. Tap the stop square to cancel. Hold the icon to see status.

Try:

- `Why does the Moon shine?`
- `Create an October 2026 calendar` — dates are computed locally; October starts Thursday. Cells default to three writing lines. Ask for Monday-start if preferred.
- `Create a flow diagram of making tea` — compact wrapping flowchart.
- `Create a 10 item checklist without labels`
- `plot y=x^2`
- `Search the weather in Lowell MA right now`
- `Create a Venn diagram comparing cats and dogs`

Answers append below existing content and may extend the page; scroll down to see them. They are image objects, not editable pen strokes. Mermaid supports flowcharts and sequence diagrams, not every Mermaid type. Venn diagrams support two or three sets. Cancellation stops the local request but cannot guarantee cancellation of server-side billing. Image generation is not supported; Claude replies with a short note instead.

## Troubleshooting

- **Unsupported OS/hardware:** only the reMarkable 2 is supported. If the icon is missing on a new OS version, check `/tmp/ink-diary-start.log` for a Qt-version message and rebuild with that version's SDK.
- **No icon:** check that installation completed and that you ran `xovi/rebuild_hashtable`, then run `xovi/start`. After a reboot you always need `xovi/start` again. Open a notebook, not just the library.
- **Check the plugin loaded:** `grep ink-diary.so /proc/$(pidof xochitl)/maps` prints a line when it's loaded. `journalctl -u xochitl -b | grep -i -E 'qmldiff|ink-diary'` shows whether the patch applied (`Processing file .../DocumentView.qml`) and any Qt-version message.
- **HTTP/API error:** the status message shows the API's error text. Check Wi-Fi, API billing, key and model name. Never share your configuration file to diagnose this.
- **Page changed:** keep the page still until the response finishes, then retry.
- **Response below the screen:** scroll down. Existing answers are not automatically replaced or corrected.
- **Diagram error:** request a simpler flowchart/sequence diagram with short labels. Limits are described in README.md.

## Remove or restore

```sh
cd /home/root/ink-diary-claude
sh uninstall.sh
```

Restart the notebook app using the start command above. Uninstallation removes the plugin and QML patch; it keeps configuration and generated answer files. To remove your stored credential as well, delete only `/home/root/.config/ink-diary/config.json` after uninstalling. Do not share your answer directory: it can contain notebook-derived text and images.

To restore this version later, verify the archive, run its installer, and restart from the library. Skip full configuration to preserve existing settings.

## Sharing and privacy

Share this repository or a release archive made from its committed files, together with this guide. Do not create a new archive from your tablet's installed configuration or answer directories. The included `config.example.json` contains only a placeholder. API credentials, notebook content, diagnostic screenshots and generated artwork are excluded from this release.

The visible notebook crop is sent to Anthropic when you tap the button. Web searches send the rewritten question in a second request. Rendering diagrams is local after transcription. See README.md for network behavior and limits, THIRD_PARTY.md for attribution, and LICENSE for redistribution terms.
