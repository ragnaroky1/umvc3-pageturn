# Snapshot the current game folder (whatever state it is in) to D:\umvc3_backup with a label.
param([string]$Label = "snapshot")
$Game = "C:\Program Files (x86)\Steam\steamapps\common\ULTIMATE MARVEL VS. CAPCOM 3"
$Stamp = Get-Date -Format "yyyy-MM-dd_HHmm"
$Dest = "D:\umvc3_backup\ULTIMATE MARVEL VS. CAPCOM 3_${Label}_$Stamp"
robocopy $Game $Dest /E /COPY:DAT /R:1 /W:1 /NP /NFL /NDL
Write-Host "Backed up to $Dest (robocopy exit $LASTEXITCODE)"
