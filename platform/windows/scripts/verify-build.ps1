param(
    [Parameter(Mandatory = $false)]
    [string]$DllPath = "build/windows/Debug/NAKLTSF.dll"
)

$ErrorActionPreference = "Stop"
$resolvedDll = (Resolve-Path -LiteralPath $DllPath).Path

$headers = (& dumpbin /headers $resolvedDll | Out-String)
if ($LASTEXITCODE -ne 0 -or $headers -notmatch "machine \(x64\)") {
    throw "NAKLTSF.dll is not a valid x64 image."
}

$exportTable = (& dumpbin /exports $resolvedDll | Out-String)
if ($LASTEXITCODE -ne 0) {
    throw "Could not read NAKLTSF.dll exports."
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
