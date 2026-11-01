# Collects an Ink Diary health report from the tablet (USB) into diagnose-output.txt. No secrets are included.
param([string]$Tablet = "root@10.11.99.1")
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$out = Join-Path $here "diagnose-output.txt"
scp -q (Join-Path $here "tools\diagnose.sh") "${Tablet}:/home/root/ink-diary-diagnose.sh"
if ($LASTEXITCODE -ne 0) { Write-Host "Cannot reach the tablet over USB. Is it awake and plugged in?" -ForegroundColor Yellow; exit 1 }
ssh $Tablet "sh /home/root/ink-diary-diagnose.sh" | Out-File -FilePath $out -Encoding utf8
Write-Host "Report saved to $out" -ForegroundColor Green
