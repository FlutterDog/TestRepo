@echo off
setlocal

chcp 65001 >nul

rem Папка для проверки.
rem Если путь не передан аргументом, проверяется текущая папка.
set "SCAN_ROOT=%~1"
if "%SCAN_ROOT%"=="" set "SCAN_ROOT=%CD%"

rem Порог длины полного пути.
rem Для Git на Windows опасная зона обычно начинается около 240 символов.
set "MAX_PATH_LEN=240"

rem Порог длины имени одного файла или папки.
set "MAX_NAME_LEN=100"

rem Имя файла отчета.
set "OUT_FILE=%CD%\long_paths_report.txt"

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
"$root = $env:SCAN_ROOT; ^
$maxPath = [int]$env:MAX_PATH_LEN; ^
$maxName = [int]$env:MAX_NAME_LEN; ^
$out = $env:OUT_FILE; ^
$result = New-Object System.Collections.Generic.List[object]; ^
$errors = New-Object System.Collections.Generic.List[string]; ^
Get-ChildItem -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue -ErrorVariable scanErrors | ForEach-Object { ^
    $full = $_.FullName; ^
    $name = $_.Name; ^
    $type = if ($_.PSIsContainer) { 'DIR ' } else { 'FILE' }; ^
    $pathLen = $full.Length; ^
    $nameLen = $name.Length; ^
    if (($pathLen -ge $maxPath) -or ($nameLen -ge $maxName)) { ^
        $result.Add([pscustomobject]@{Type=$type; PathLength=$pathLen; NameLength=$nameLen; FullPath=$full}) ^
    } ^
}; ^
foreach ($e in $scanErrors) { $errors.Add($e.ToString()) } ^
$lines = New-Object System.Collections.Generic.List[string]; ^
$lines.Add('SCAN ROOT: ' + $root); ^
$lines.Add('MAX FULL PATH LENGTH: ' + $maxPath); ^
$lines.Add('MAX FILE/FOLDER NAME LENGTH: ' + $maxName); ^
$lines.Add(''); ^
$lines.Add('TYPE | PATH_LEN | NAME_LEN | FULL_PATH'); ^
$lines.Add('------------------------------------------------------------'); ^
$result | Sort-Object PathLength -Descending | ForEach-Object { ^
    $lines.Add(('{0} | {1} | {2} | {3}' -f $_.Type, $_.PathLength, $_.NameLength, $_.FullPath)) ^
}; ^
if ($errors.Count -gt 0) { ^
    $lines.Add(''); ^
    $lines.Add('SCAN ERRORS:'); ^
    $lines.Add('------------------------------------------------------------'); ^
    foreach ($err in $errors) { $lines.Add($err) } ^
}; ^
$lines | Set-Content -LiteralPath $out -Encoding UTF8; ^
Write-Host ('Report saved to: ' + $out); ^
Write-Host ('Found risky paths: ' + $result.Count);"

echo.
echo Report saved to:
echo %OUT_FILE%
echo.
pause