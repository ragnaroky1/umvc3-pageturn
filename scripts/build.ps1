# Builds UMvC3PageTurn.asi with MSVC. Output: build\UMvC3PageTurn.asi
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$VcVars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
New-Item -ItemType Directory -Force "$Root\build" | Out-Null
$src = @("$Root\src\dllmain.cpp") + (Get-ChildItem "$Root\src\third_party\minhook\src" -Recurse -Filter *.c | ForEach-Object { $_.FullName })
$srcList = ($src | ForEach-Object { "`"$_`"" }) -join " "
$cmd = "call `"$VcVars`" >nul && cd /d `"$Root\build`" && cl /nologo /O2 /W3 /EHsc /MT /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS /I`"$Root\src\third_party\minhook\include`" $srcList /link /DLL /OUT:UMvC3PageTurn.asi /EXPORT:InitializeASI user32.lib"
cmd /c $cmd 2>&1 | Tee-Object "$Rootbuildbuild.log"
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }
Get-Item "$Root\build\UMvC3PageTurn.asi" | Select-Object Name, Length, LastWriteTime
