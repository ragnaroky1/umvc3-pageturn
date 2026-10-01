# Builds the release zip: dist\UMvC3PageTurn-<version>.zip
# Contents: our .asi, our blank texture, README, creator icon guide, MinHook license. Nothing from Capcom / CE / ASI loader.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$ver = (Select-String -Path "$Root\src\dllmain.cpp" -Pattern '#define PT_VERSION "([^"]+)"').Matches[0].Groups[1].Value
$stage = "$Root\dist\UMvC3PageTurn-$ver"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force "$stage\nativePCx64\ui\PageTurn", "$stage\docs" | Out-Null
Copy-Item "$Root\build\UMvC3PageTurn.asi" $stage
Copy-Item "$Root\assets\nativePCx64\ui\PageTurn\*" "$stage\nativePCx64\ui\PageTurn\"
Copy-Item "$Root\README.md" $stage
Copy-Item "$Root\docs\ICONS_FOR_CREATORS.md" "$stage\docs\"
Copy-Item "$Root\scripts\make_icon.py" "$stage\docs\"
Copy-Item "$Root\src\third_party\minhook\LICENSE.txt" "$stage\LICENSE-MinHook.txt"
$zip = "$Root\dist\UMvC3PageTurn-$ver.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path "$stage\*" -DestinationPath $zip
Get-ChildItem $stage -Recurse -File | ForEach-Object { $_.FullName.Substring($stage.Length + 1) }
Get-Item $zip | Select-Object Name, Length
