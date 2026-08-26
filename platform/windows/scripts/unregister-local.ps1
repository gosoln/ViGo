param(
    [Parameter(Mandatory = $false)]
    [string]$DllPath = "build/windows/Debug/NAKLTSF.dll"
)

$ErrorActionPreference = "Stop"

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
$isAdministrator = $principal.IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator
)
if (-not $isAdministrator) {
    throw "Run this script from an elevated x64 PowerShell session."
}
if (-not [Environment]::Is64BitProcess) {
    throw "Use 64-bit PowerShell to unregister the x64 TSF DLL."
}

$resolvedDll = (Resolve-Path -LiteralPath $DllPath).Path
$process = Start-Process `
    -FilePath "$env:SystemRoot\System32\regsvr32.exe" `
    -ArgumentList "/u", "/s", "`"$resolvedDll`"" `
    -Wait `
    -PassThru
if ($process.ExitCode -ne 0) {
    throw "regsvr32 /u failed with exit code $($process.ExitCode)."
}

Write-Host "Unregistered NAKL TSF profile. Sign out to unload remaining DLL instances."
