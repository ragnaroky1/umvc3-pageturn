# Copies the ASI loader (dinput8.dll) and our .asi into the game folder. Does not touch any other file.
$Game = "C:\Program Files (x86)\Steam\steamapps\common\ULTIMATE MARVEL VS. CAPCOM 3"
$Root = Split-Path -Parent $PSScriptRoot
if (Get-Process umvc3 -ErrorAction SilentlyContinue) { throw "Close the game first." }
if (-not (Test-Path "$Game\dinput8.dll")) {
    Copy-Item "C:\Tools\asiloader\dinput8.dll" "$Game\dinput8.dll"
    Write-Host "installed Ultimate ASI Loader as dinput8.dll"
} else { Write-Host "dinput8.dll already present (left as is)" }
Copy-Item "$Root\build\UMvC3PageTurn.asi" "$Game\UMvC3PageTurn.asi" -Force
Write-Host "installed UMvC3PageTurn.asi"
New-Item -ItemType Directory -Force "$Game\nativePCx64\ui\PageTurn" | Out-Null
Copy-Item "$Root\assets\nativePCx64\ui\PageTurn\*" "$Game\nativePCx64\ui\PageTurn\" -Force
Write-Host "installed PageTurn assets (nativePCx64\ui\PageTurn)"
