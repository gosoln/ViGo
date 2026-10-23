param(
    [Parameter(Mandatory = $false)]
    [string]$DllPath = "build/windows/Debug/ViGoTSF.dll",

    [Parameter(Mandatory = $false)]
    [string]$SettingsPath = "build/windows/Debug/ViGoSettings.exe"
)

$ErrorActionPreference = "Stop"

function Find-Dumpbin {
    $command = Get-Command dumpbin -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $found = & $vswhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -find "VC\Tools\MSVC\**\bin\Hostx64\x64\dumpbin.exe" | Select-Object -First 1
        if ($found) { return $found }
    }
    throw "dumpbin was not found. Run from a Visual Studio Developer PowerShell."
}
$dumpbin = Find-Dumpbin
$resolvedDll = (Resolve-Path -LiteralPath $DllPath).Path

$headers = (& $dumpbin /headers $resolvedDll | Out-String)
if ($LASTEXITCODE -ne 0 -or $headers -notmatch "machine \(x64\)") {
    throw "ViGoTSF.dll is not a valid x64 image."
}

$exportTable = (& $dumpbin /exports $resolvedDll | Out-String)
if ($LASTEXITCODE -ne 0) {
    throw "Could not read ViGoTSF.dll exports."
}
foreach ($requiredExport in @(
    "DllCanUnloadNow",
    "DllGetClassObject",
    "DllRegisterServer",
    "DllUnregisterServer"
)) {
    if ($exportTable -notmatch "\b$requiredExport\b") {
        throw "Missing COM export: $requiredExport"
    }
}

Write-Host "PASS: x64 TSF DLL and COM exports are present."

$resolvedSettings = (Resolve-Path -LiteralPath $SettingsPath).Path
$settingsHeaders = (& $dumpbin /headers $resolvedSettings | Out-String)
if ($LASTEXITCODE -ne 0 -or $settingsHeaders -notmatch "machine \(x64\)") {
    throw "ViGoSettings.exe is not a valid x64 image."
}

Write-Host "PASS: x64 settings application is present."
