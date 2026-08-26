param(
    [Parameter(Mandatory = $false)]
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [Parameter(Mandatory = $false)]
    [switch]$AcceptWix7Eula
)

$ErrorActionPreference = "Stop"

if (-not [Environment]::Is64BitOperatingSystem) {
    throw "NAKL Windows release builds require Windows x64."
}
if (-not $AcceptWix7Eula) {
    throw @"
WiX 7 requires explicit acceptance of the OSMF EULA. Review
https://docs.firegiant.com/wix/osmf/ and rerun with -AcceptWix7Eula only when
the person or organization building NAKL has satisfied those terms.
"@
}

$repository = (Resolve-Path (Join-Path $PSScriptRoot "../../..")).Path
$buildDirectory = Join-Path $repository "build/windows"
$installerProject = Join-Path $repository "platform/windows/installer/NAKL.wixproj"
$nativeOutput = Join-Path $buildDirectory $Configuration
$coreLibrary = Join-Path $repository "target/release/nakl_core.lib"

Push-Location $repository
try {
    cargo build -p nakl-core --release
    cmake -S platform/windows -B $buildDirectory -A x64 -DBUILD_TESTING=ON `
        -DNAKL_CORE_LIBRARY=$coreLibrary
    cmake --build $buildDirectory --config $Configuration
    ctest --test-dir $buildDirectory -C $Configuration --output-on-failure
    & platform/windows/scripts/verify-build.ps1 `
        -DllPath (Join-Path $nativeOutput "NAKLTSF.dll") `
        -SettingsPath (Join-Path $nativeOutput "NAKLSettings.exe")
    dotnet build $installerProject `
        -c $Configuration `
        -p:AcceptEula=wix7 `
        -p:NAKLBuildDir=$nativeOutput
} finally {
    Pop-Location
}

Write-Host "Release artifacts are under $nativeOutput and platform/windows/installer/bin/$Configuration."
