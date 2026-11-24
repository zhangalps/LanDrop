# One-click Windows Release Build & Single-EXE Installer Packaging Script
# Outputs the standalone GUI setup executable into installer/output/

param(
    [string]$QtBinDir = "F:\Qt\6.12.0\mingw_64\bin",
    [string]$MinGwBinDir = "F:\Qt\Tools\mingw1310_64\bin",
    [string]$CMakeBinDir = "F:\Qt\Tools\CMake_64\bin",
    [string]$Version = "1.0.0"
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$QtPrefixPath = Split-Path -Parent $QtBinDir
$BuildDir = Join-Path $ProjectRoot "build\windows_release"
$DistAppDir = Join-Path $PSScriptRoot "dist\LanDrop"
$PayloadZip = Join-Path $PSScriptRoot "dist\LanDrop-Payload.zip"
$OutputRoot = Join-Path $PSScriptRoot "output"
$SetupExePath = Join-Path $OutputRoot "LanDrop-Setup-$Version.exe"
$IconPath = Join-Path $ProjectRoot "resources\icons\landrop.ico"
$SetupSrcDir = Join-Path $PSScriptRoot "setup_src"
$CscExe = "C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe"

$env:Path = "$MinGwBinDir;$QtBinDir;$CMakeBinDir;" + $env:Path

Write-Host "[1/4] Configuring & building LanDrop (Release)..." -ForegroundColor Cyan
cmake -S $ProjectRoot -B $BuildDir -G "MinGW Makefiles" `
    -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_PREFIX_PATH=$QtPrefixPath" `
    "-DCMAKE_CXX_COMPILER=$MinGwBinDir\g++.exe" `
    "-DCMAKE_MAKE_PROGRAM=$MinGwBinDir\mingw32-make.exe" `
    -DLANDROP_BUILD_TESTS=OFF
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
cmake --build $BuildDir -j 8
if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }

Write-Host "[2/4] Deploying Qt6 & QML runtime via windeployqt..." -ForegroundColor Cyan
if (Test-Path $DistAppDir) { Remove-Item -Recurse -Force $DistAppDir }
New-Item -ItemType Directory -Force -Path $DistAppDir | Out-Null
Copy-Item (Join-Path $BuildDir "LanDrop.exe") $DistAppDir -Force

& "$QtBinDir\windeployqt.exe" `
    --release `
    --qmldir (Join-Path $ProjectRoot "qml") `
    --no-translations `
    --no-opengl-sw `
    (Join-Path $DistAppDir "LanDrop.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed." }

foreach ($dll in @("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")) {
    $dllPath = Join-Path $MinGwBinDir $dll
    if (Test-Path $dllPath) { Copy-Item $dllPath $DistAppDir -Force }
}

Write-Host "[3/4] Packing payload archive..." -ForegroundColor Cyan
if (Test-Path $PayloadZip) { Remove-Item -Force $PayloadZip }
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory(
    $DistAppDir,
    $PayloadZip,
    [System.IO.Compression.CompressionLevel]::Optimal,
    $false
)

Write-Host "[4/4] Building standalone GUI installer EXE..." -ForegroundColor Cyan
if (Test-Path $OutputRoot) { Remove-Item -Recurse -Force $OutputRoot }
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$iscc = Get-Command iscc -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
if (-not $iscc -and (Test-Path "C:\Program Files (x86)\Inno Setup 6\ISCC.exe")) {
    $iscc = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
}

if ($iscc) {
    & $iscc (Join-Path $PSScriptRoot "LanDrop.iss")
    if ($LASTEXITCODE -ne 0) { throw "Inno Setup compilation failed." }
} elseif (Test-Path $CscExe) {
    $fxDir = Split-Path -Parent $CscExe
    $srcFiles = @(
        (Join-Path $SetupSrcDir "Program.cs"),
        (Join-Path $SetupSrcDir "InstallEngine.cs"),
        (Join-Path $SetupSrcDir "InstallerForm.cs")
    )
    & $CscExe `
        /nologo `
        /target:winexe `
        /optimize+ `
        /utf8output `
        "/win32icon:$IconPath" `
        "/out:$SetupExePath" `
        "/r:$fxDir\System.Windows.Forms.dll" `
        "/r:$fxDir\System.Drawing.dll" `
        "/r:$fxDir\System.IO.Compression.dll" `
        "/r:$fxDir\System.IO.Compression.FileSystem.dll" `
        "/resource:$PayloadZip,LanDrop.Payload.zip" `
        $srcFiles
    if ($LASTEXITCODE -ne 0) { throw "C# installer compilation failed." }
} else {
    throw "Neither Inno Setup 6 (ISCC.exe) nor .NET Framework csc.exe was found."
}

$sizeMb = [Math]::Round((Get-Item $SetupExePath).Length / 1MB, 2)
Write-Host "Done! Single-file Windows installer ready: $SetupExePath ($sizeMb MB)" -ForegroundColor Green
