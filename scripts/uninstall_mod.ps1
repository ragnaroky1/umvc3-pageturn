# Removes our .asi. Removes dinput8.dll only if no other .asi remains (Clone Engine needs it).
$Game = "C:\Program Files (x86)\Steam\steamapps\common\ULTIMATE MARVEL VS. CAPCOM 3"
Remove-Item "$Game\UMvC3PageTurn.asi" -ErrorAction SilentlyContinue
Remove-Item "$Game\UMvC3PageTurn.log" -ErrorAction SilentlyContinue
$others = Get-ChildItem $Game -Filter *.asi -Recurse -ErrorAction SilentlyContinue
if (-not $others) { Remove-Item "$Game\dinput8.dll" -ErrorAction SilentlyContinue; Write-Host "removed dinput8.dll (no other ASI mods present)" }
Write-Host "UMvC3PageTurn removed"
