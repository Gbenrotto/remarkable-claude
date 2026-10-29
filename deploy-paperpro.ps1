# Deploy Ink Diary to a reMarkable Paper Pro over the USB cable, from Windows PowerShell.
#
#     powershell -ExecutionPolicy Bypass -File .\deploy-paperpro.ps1
#     powershell -ExecutionPolicy Bypass -File .\deploy-paperpro.ps1 -Bundle "$env:USERPROFILE\Downloads\ink-diary-paperpro.zip"
#
# The bundle comes from build-paperpro.sh (dist\ink-diary-paperpro.tar.gz) or from the GitHub
# Actions artifact (a .zip that contains that .tar.gz); -Bundle accepts either. Needs ssh/scp
# (built into Windows 10/11) and the tablet connected over USB in developer mode. If remagic
# set up an SSH key, there are no password prompts.
param(
    [string]$Tablet = "root@10.11.99.1",
    [string]$Bundle = "",
    [switch]$Reconfigure,        # re-enter the API key/model/timezone even if a configuration exists
    [switch]$SkipConfigure,      # never run configure.sh (even on a first install)
    [switch]$SkipHashtable       # skip xovi/rebuild_hashtable (already done for this OS version)
)
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$unpack = $null
if (-not (Get-Command ssh -ErrorAction SilentlyContinue)) { Write-Error "ssh was not found. Install the OpenSSH Client (Settings > System > Optional features) and run this again."; exit 1 }
if (-not $Bundle) { $Bundle = Join-Path $here "dist\ink-diary-paperpro.tar.gz" }
if (-not (Test-Path $Bundle)) { Write-Error "Not found: $Bundle. Run build-paperpro.sh in WSL, or download the GitHub Actions artifact and pass it with -Bundle."; exit 1 }
if ($Bundle -like "*.zip") {
    $unpack = Join-Path $env:TEMP ("ink-diary-" + [guid]::NewGuid().ToString("N"))
    Expand-Archive -Path $Bundle -DestinationPath $unpack -Force
    $inner = Get-ChildItem -Path $unpack -Recurse -Filter "ink-diary-paperpro.tar.gz" | Select-Object -First 1
    if (-not $inner) { Write-Error "The zip does not contain ink-diary-paperpro.tar.gz"; exit 1 }
    $Bundle = $inner.FullName
}

function Step($text) { Write-Host "`n==> $text" -ForegroundColor Cyan }
function Remote($command) {
    ssh -o ConnectTimeout=10 $Tablet $command
    if ($LASTEXITCODE -ne 0) { throw "Command failed on the tablet: $command" }
}

Step "Checking the tablet at $Tablet"
$lines = ssh -o ConnectTimeout=10 $Tablet "uname -m; cat /sys/devices/soc0/machine 2>/dev/null; sed -n 's/^REMARKABLE_RELEASE_VERSION=//p' /usr/share/remarkable/update.conf; test -d /home/root/xovi && echo xovi-present || echo xovi-missing"
if ($LASTEXITCODE -ne 0) { Write-Error "Cannot reach the tablet. Is it awake, connected over USB, and in developer mode?"; exit 1 }
$info = ($lines -join "`n")
$lines | ForEach-Object { Write-Host "  $_" }
if ($info -notmatch "aarch64") { Write-Error "This is not a 64-bit tablet; the Paper Pro bundle will not load."; exit 1 }
if ($info -match "xovi-missing") { Write-Error "xovi is not installed on the tablet. Run remagic setup (or install Vellum + xovi) first."; exit 1 }
if ($info -notmatch "3\.28\.0\.172") { Write-Host "  Note: the bundle was built for OS 3.28.0.172; a different OS needs a rebuild with its SDK." -ForegroundColor Yellow }

Step "Copying the bundle"
scp $Bundle "${Tablet}:/home/root/ink-diary-paperpro.tar.gz"
if ($LASTEXITCODE -ne 0) { throw "scp failed" }

Step "Unpacking, verifying checksums and installing"
Remote "rm -rf /home/root/ink-diary-claude && mkdir -p /home/root/ink-diary-claude && tar -xzf /home/root/ink-diary-paperpro.tar.gz -C /home/root/ink-diary-claude && rm /home/root/ink-diary-paperpro.tar.gz"
Remote "cd /home/root/ink-diary-claude && { sha256sum -c SHA256SUMS > /tmp/ink-diary-sums.log 2>&1 || { grep -v ': OK`$' /tmp/ink-diary-sums.log; echo 'Checksum verification failed; nothing installed'; exit 1; }; } && echo 'All files verified' && sh install.sh"

$configured = ssh $Tablet "test -s /home/root/.config/ink-diary/config.json && echo yes || echo no"
if (-not $SkipConfigure -and ($Reconfigure -or (($configured -join "") -notmatch "yes"))) {
    Step "API key, model and timezone (typed on the tablet side; the key is hidden)"
    ssh -t $Tablet "sh /home/root/.config/ink-diary/configure.sh"
    if ($LASTEXITCODE -ne 0) { throw "Configuration was not saved. Run the script again, or ssh in and run configure.sh by hand." }
}

if (-not $SkipHashtable) {
    Step "Rebuilding xovi's hashtable (press Enter when it asks; the tablet UI restarts once)"
    ssh -t $Tablet "bash /home/root/xovi/rebuild_hashtable"
    if ($LASTEXITCODE -ne 0) { Write-Host "  rebuild_hashtable reported an error; the button may not appear. See INSTALL-PAPER-PRO.md." -ForegroundColor Yellow }
}

Step "Starting xovi with Ink Diary loaded"
Remote "bash /home/root/xovi/start"
# xochitl can run more than one process (a renderer child), so look in every one of them,
# and give the restart up to 30 seconds.
$probe = "for p in `$(pidof xochitl); do grep -q ink-diary.so /proc/`$p/maps 2>/dev/null && { echo loaded; exit 0; }; done; echo waiting"
$loaded = $false
for ($i = 0; $i -lt 15 -and -not $loaded; $i++) {
    Start-Sleep -Seconds 2
    $loaded = ((ssh -o ConnectTimeout=10 $Tablet $probe) -join "") -match "loaded"
}
if ($loaded) {
    Write-Host "`nInk Diary is loaded. Open a notebook in portrait; the Claude button is top-right under the close button." -ForegroundColor Green
} else {
    Write-Host "`nThe plugin is not loaded in the notebook app. Check the log with:" -ForegroundColor Yellow
    Write-Host ('  ssh ' + $Tablet + ' "journalctl -u xochitl -b --no-pager | grep -i -E ''ink-diary|qmldiff|xovi'' | tail -40"')
    Write-Host "If the UI fell back to stock, move the plugin aside to confirm it is the cause:"
    Write-Host ('  ssh ' + $Tablet + ' "mv /home/root/xovi/extensions.d/ink-diary.so /home/root/ink-diary.so.off && bash /home/root/xovi/start"')
}
if ($unpack -and (Test-Path $unpack)) { Remove-Item -Recurse -Force $unpack }
Write-Host ('After every reboot: triple-press the power button (tripletap) or run  ssh ' + $Tablet + ' "bash /home/root/xovi/start"')
