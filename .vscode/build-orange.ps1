[CmdletBinding()]
param(
    [string]$ToolchainRoot = "$env:LOCALAPPDATA\ArmGNUToolchain\10.3-2021.07-aarch64-linux",
    [string]$Sysroot
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path "$PSScriptRoot\..").Path
if (-not $Sysroot) {
    $Sysroot = Join-Path $projectRoot ".sysroot-orange"
}
$toolBin = Join-Path $ToolchainRoot "bin"
$cxx = Join-Path $toolBin "aarch64-none-linux-gnu-g++.exe"
$ar = Join-Path $toolBin "aarch64-none-linux-gnu-ar.exe"
$readelf = Join-Path $toolBin "aarch64-none-linux-gnu-readelf.exe"
$targetLib = Join-Path $Sysroot "usr\lib\aarch64-linux-gnu"
$buildDir = Join-Path $projectRoot ".build-orange"

foreach ($path in @($cxx, $ar, $readelf, "$Sysroot\usr\include", $targetLib)) {
    if (-not (Test-Path $path)) {
        throw "Required path is missing: $path"
    }
}

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$common = @(
    "-DGGK_DISABLE_SET_LOCAL_NAME=1",
    "-fPIC", "-Wall", "-Wextra", "-std=c++2a", "-pthread", "-g", "-O2",
    "-I$projectRoot\src",
    "-I$projectRoot\include",
    "-I$Sysroot\usr\include\glib-2.0",
    "-I$targetLib\glib-2.0\include",
    "-I$Sysroot\usr\include\dbus-1.0",
    "-I$targetLib\dbus-1.0\include",
    "-idirafter", "$Sysroot\usr\include"
)

$sources = @(
    "DBusInterface.cpp", "DBusMethod.cpp", "DBusObject.cpp",
    "GattCharacteristic.cpp", "GattDescriptor.cpp", "GattInterface.cpp",
    "GattProperty.cpp", "GattService.cpp", "Gobbledegook.cpp",
    "HciAdapter.cpp", "HciSocket.cpp", "Init.cpp", "Logger.cpp",
    "Mgmt.cpp", "Server.cpp", "ServerUtils.cpp", "Utils.cpp"
)

$objects = foreach ($source in $sources) {
    $object = Join-Path $buildDir "$([IO.Path]::GetFileNameWithoutExtension($source)).o"
    & $cxx @common -c "$projectRoot\src\$source" -o $object
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $source" }
    $object
}

$library = Join-Path $projectRoot "libggk-orange.a"
& $ar rcs $library @objects
if ($LASTEXITCODE -ne 0) { throw "Unable to create $library" }

$output = Join-Path $projectRoot "standalone-orange"
& $cxx @common `
    "$projectRoot\src\standalone.cpp" `
    $library `
    "$targetLib\libstdc++.so.6" `
    "$targetLib\libgcc_s.so.1" `
    "-Wl,-rpath-link,$targetLib" `
    -o $output `
    "$targetLib\libgio-2.0.so" `
    "$targetLib\libgobject-2.0.so" `
    "$targetLib\libgmodule-2.0.so" `
    "$targetLib\libglib-2.0.so" `
    "$targetLib\libdbus-1.so" `
    "-pthread"
if ($LASTEXITCODE -ne 0) { throw "Linking failed: $output" }

& $readelf -h $output | Select-String "Class:|Machine:"
& $readelf --version-info $output |
    Select-String "Name: GLIBC_" |
    ForEach-Object Line |
    Sort-Object -Unique

Write-Host "Built $library"
Write-Host "Linked $output"
