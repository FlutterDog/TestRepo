@echo off
setlocal EnableExtensions

chcp 65001 >nul

rem Если путь не передан, проверяем папку, где лежит сам bat-файл.
set "SCAN_ROOT=%~1"
if "%SCAN_ROOT%"=="" set "SCAN_ROOT=%~dp0"

for %%I in ("%SCAN_ROOT%") do set "SCAN_ROOT=%%~fI"

rem Порог длины полного пути.
set "MAX_PATH_LEN=240"

rem Порог длины имени одного файла или папки.
set "MAX_NAME_LEN=100"

rem Отчет кладем рядом с bat-файлом.
set "OUT_FILE=%~dp0long_paths_report.txt"

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$root=$env:SCAN_ROOT; $maxPath=[int]$env:MAX_PATH_LEN; $maxName=[int]$env:MAX_NAME_LEN; $out=$env:OUT_FILE; $result=New-Object System.Collections.Generic.List[object]; $scanErrors=@(); $items=Get-ChildItem -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue -ErrorVariable scanErrors; foreach($item in $items){$full=$item.FullName;$name=$item.Name;$type=if($item.PSIsContainer){'DIR '}else{'FILE'};$pathLen=$full.Length;$nameLen=$name.Length;if(($pathLen -ge $maxPath) -or ($nameLen -ge $maxName)){[void]$result.Add([pscustomobject]@{Type=$type;PathLength=$pathLen;NameLength=$nameLen;FullPath=$full})}}; $lines=New-Object System.Collections.Generic.List[string]; [void]$lines.Add('SCAN ROOT: '+$root); [void]$lines.Add('MAX FULL PATH LENGTH: '+$maxPath); [void]$lines.Add('MAX FILE/FOLDER NAME LENGTH: '+$maxName); [void]$lines.Add(''); [void]$lines.Add('TYPE | PATH_LEN | NAME_LEN | FULL_PATH'); [void]$lines.Add('------------------------------------------------------------'); foreach($r in ($result | Sort-Object PathLength -Descending)){[void]$lines.Add(('{0} | {1} | {2} | {3}' -f $r.Type,$r.PathLength,$r.NameLength,$r.FullPath))}; if($scanErrors.Count -gt 0){[void]$lines.Add('');[void]$lines.Add('SCAN ERRORS:');[void]$lines.Add('------------------------------------------------------------');foreach($err in $scanErrors){[void]$lines.Add($err.ToString())}}; Set-Content -LiteralPath $out -Encoding UTF8 -Value $lines; Write-Host ('Report saved to: '+$out); Write-Host ('Found risky paths: '+$result.Count)"

echo.
echo Report saved to:
echo %OUT_FILE%
echo.
pause