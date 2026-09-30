# Restores the clean (unmodded) UMvC3 install from the D: backup.
# Usage: run in PowerShell as the normal user. Close the game and Steam first.
$Game   = "C:\Program Files (x86)\Steam\steamapps\common\ULTIMATE MARVEL VS. CAPCOM 3"
$Backup = "D:\umvc3_backup\ULTIMATE MARVEL VS. CAPCOM 3_clean_2026-09-30"
if (-not (Test-Path $Backup)) { Write-Error "Backup not found at $Backup"; exit 1 }
if (Get-Process umvc3 -ErrorAction SilentlyContinue) { Write-Error "Close the game first."; exit 1 }
Write-Host "Mirroring clean backup over game folder (removes any modded files)..."
robocopy $Backup $Game /MIR /COPY:DAT /R:2 /W:2 /NP /NFL /NDL
if ($LASTEXITCODE -ge 8) { Write-Error "robocopy failed with $LASTEXITCODE"; exit 1 }
Write-Host "Restore complete. Verify in Steam with 'Verify integrity of game files' if in doubt."
