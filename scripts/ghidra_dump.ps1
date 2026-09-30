# Usage: .\scripts\ghidra_dump.ps1 out.txt 0x140361fd0 0x140323920 ...
param([string]$Out, [Parameter(ValueFromRemainingArguments=$true)][string[]]$Addrs)
$env:JAVA_HOME="C:\Tools\jdk-21.0.12.1+1"; $env:PATH="$env:JAVA_HOME\bin;$env:PATH"
$OutAbs = [System.IO.Path]::GetFullPath($Out)
& C:\Tools\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat C:\Tools\ghidra_projects umvc3 -process umvc3.exe.unpacked.exe -noanalysis -scriptPath C:\Tools\ghidra_scripts -postScript DumpFuncs.java $OutAbs @Addrs 2>&1 | Select-String -Pattern "ERROR|Exception|DumpFuncs" | Select-Object -First 10
