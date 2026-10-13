param(
    [Parameter(Mandatory = $true)]
    [string]$QtRoot,
    [string]$VcpkgRoot = "",
    [ValidateSet("Release", "Debug", "RelWithDebInfo")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build/windows"
$DistDir = Join-Path $ProjectRoot "dist/windows"

if (-not (Test-Path (Join-Path $QtRoot "bin/windeployqt.exe"))) {
    throw "QtRoot must point to an MSVC 2022 desktop Qt installation, for example C:\Qt\6.8.3\msvc2022_64"
}

$ConfigureArgs = @(
    "-S", $ProjectRoot,
    "-B", $BuildDir,
    "-G", "Ninja Multi-Config",
    "-DCMAKE_PREFIX_PATH=$QtRoot",
    "-DCHROMADESK_BUILD_TESTS=ON"
)

if ($VcpkgRoot) {
    $Toolchain = Join-Path $VcpkgRoot "scripts/buildsystems/vcpkg.cmake"
    if (-not (Test-Path $Toolchain)) {
        throw "VcpkgRoot does not contain scripts/buildsystems/vcpkg.cmake"
    }
    $ConfigureArgs += "-DCMAKE_TOOLCHAIN_FILE=$Toolchain"
    $ConfigureArgs += "-DVCPKG_TARGET_TRIPLET=x64-windows"
} else {
    $ConfigureArgs += "-DCHROMADESK_ENABLE_RAW=OFF"
    Write-Warning "LibRaw disabled. Pass -VcpkgRoot to include camera RAW support."
}

cmake @ConfigureArgs
cmake --build $BuildDir --config $Configuration --parallel
ctest --test-dir $BuildDir -C $Configuration --output-on-failure

New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
$Executable = Join-Path $BuildDir "$Configuration/ChromaDesk.exe"
Copy-Item $Executable $DistDir -Force
$DeployMode = "--$($Configuration.ToLower())"
& (Join-Path $QtRoot "bin/windeployqt.exe") $DeployMode --compiler-runtime --no-translations $DistDir

if ($VcpkgRoot) {
    $VcpkgBin = if ($Configuration -eq "Debug") {
        Join-Path $ProjectRoot "vcpkg_installed/x64-windows/debug/bin"
    } else {
        Join-Path $ProjectRoot "vcpkg_installed/x64-windows/bin"
    }
    if (Test-Path $VcpkgBin) {
        Copy-Item (Join-Path $VcpkgBin "*.dll") $DistDir -Force
    }
}

Compress-Archive -Path (Join-Path $DistDir "*") `
    -DestinationPath (Join-Path $ProjectRoot "dist/ChromaDesk-Windows-x64.zip") -Force
Write-Host "Package: $ProjectRoot\dist\ChromaDesk-Windows-x64.zip"
