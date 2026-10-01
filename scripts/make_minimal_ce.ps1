# Rebuilds the game folder as: clean vanilla + Clone Engine runtime (+ColorExpansion) + the pack's data files + a Characters.ini
# trimmed to the first N characters + PageTurn. (mag_patch.asi and mod_enable_damage_counter.asi are left out.)
# Usage: .\scripts\make_minimal_ce.ps1 [-Count 7]
param([int]$Count = 7)
$ErrorActionPreference = "Stop"
$Game   = "C:\Program Files (x86)\Steam\steamapps\common\ULTIMATE MARVEL VS. CAPCOM 3"
$Backup = "D:\umvc3_backup\ULTIMATE MARVEL VS. CAPCOM 3_clean_2026-09-30"
$CE     = "D:\umvc3_ce\build\UMvC3 Community Edition Build"
$Root   = Split-Path -Parent $PSScriptRoot
if (Get-Process umvc3 -ErrorAction SilentlyContinue) { throw "Close the game first." }

Write-Host "1/4 restoring clean vanilla (mirror from backup)..."
robocopy $Backup $Game /MIR /COPY:DAT /R:1 /W:1 /NP /NFL /NDL | Out-Null
if ($LASTEXITCODE -ge 8) { throw "restore failed ($LASTEXITCODE)" }

Write-Host "2/4 Clone Engine runtime..."
Copy-Item "$CE\dinput8.dll", "$CE\CloneEngine.asi" $Game -Force
# ColorExpansion.asi remaps costume/colour ids (the "255" portrait files); the game needs it in Arcade mode. It is a debug build and needs the shipped debug CRT DLLs.
Copy-Item "$CE\ColorExpansion.asi", "$CE\ColorExpansion.ini", "$CE\msvcp140d.dll", "$CE\vcruntime140d.dll", "$CE\ucrtbased.dll" $Game -Force
New-Item -ItemType Directory -Force "$Game\nativePCx64\CloneEngine" | Out-Null
Copy-Item "$CE\nativePCx64\CloneEngine\*" "$Game\nativePCx64\CloneEngine\" -Force
New-Item -ItemType Directory -Force "$Game\nativePCx64\ColorExpand" | Out-Null
Copy-Item "$CE\nativePCx64\ColorExpand\*" "$Game\nativePCx64\ColorExpand\" -Force

# Characters.ini: first $Count characters plus any children they reference
$ini = Get-Content "$CE\Characters.ini"
$blocks = @(); $cur = $null
foreach ($line in $ini) {
    if ($line -match '^\[Character(\d+)\]') { $cur = [ordered]@{ header = $line; lines = @(); id = ""; children = @() }; $blocks += $cur; continue }
    if ($null -eq $cur) { continue }
    $cur.lines += $line
    if ($line -match '^CharacterID=(.*)$') { $cur.id = $Matches[1].Trim() }
    if ($line -match '^Child\d+=(.+)$') { $cur.children += $Matches[1].Trim() }
}
$want = New-Object System.Collections.Generic.List[string]
$byId = @{}; foreach ($b in $blocks) { $byId[$b.id] = $b }
$childIds = @{}; foreach ($b in $blocks) { foreach ($c in $b.children) { $childIds[$c] = $true } }
foreach ($b in $blocks) { if ($want.Count -ge $Count) { break }; if ($childIds.ContainsKey($b.id)) { continue }; $want.Add($b.id); foreach ($c in $b.children) { if (-not $want.Contains($c)) { $want.Add($c) } } }
$out = @(); $n = 1
foreach ($id in $want) { $b = $byId[$id]; $out += "[Character$n]"; $out += $b.lines; $n++ }
Set-Content "$Game\Characters.ini" $out
Write-Host ("   characters: " + ($want -join ", "))

Write-Host "3/4 pack data (whole nativePCx64 tree: extra costume/colour files are needed by ColorExpansion and Arcade CPU picks)..."
robocopy "$CE\nativePCx64" "$Game\nativePCx64" /E /NP /NFL /NDL | Out-Null
Write-Host ("   files in nativePCx64: " + (Get-ChildItem "$Game\nativePCx64" -Recurse -File | Measure-Object).Count)

Write-Host "4/4 PageTurn..."
Copy-Item "$Root\build\UMvC3PageTurn.asi" "$Game\UMvC3PageTurn.asi" -Force
New-Item -ItemType Directory -Force "$Game\nativePCx64\ui\PageTurn" | Out-Null
Copy-Item "$Root\assets\nativePCx64\ui\PageTurn\*" "$Game\nativePCx64\ui\PageTurn\" -Force
Get-ChildItem $Game | Select-Object Name, Length
