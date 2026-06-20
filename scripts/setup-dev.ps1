$ErrorActionPreference = 'Stop'

Write-Host "OmniPresence Dev Setup" -ForegroundColor Cyan
Write-Host ""

# Check for CMake
Write-Host "Checking for CMake..." -ForegroundColor Yellow
try {
    $cmakeVersion = cmake --version 2>&1 | Select-Object -First 1
    Write-Host "✓ CMake found: $cmakeVersion" -ForegroundColor Green
} catch {
    Write-Host "✗ CMake not found. Install from https://cmake.org/download/" -ForegroundColor Red
    exit 1
}

# Check for C++ toolchain (MSVC)
Write-Host "Checking for C++ toolchain..." -ForegroundColor Yellow
try {
    $msvcVersion = cl.exe 2>&1 | Select-Object -First 1
    Write-Host "✓ MSVC found" -ForegroundColor Green
} catch {
    Write-Host "✗ MSVC not found. Install Visual Studio with C++ workload from https://visualstudio.microsoft.com/" -ForegroundColor Red
    exit 1
}

# Check for Qt6
Write-Host "Checking for Qt6..." -ForegroundColor Yellow
$qtFound = $false
if (Test-Path "C:\Qt\6*" -PathType Container) {
    Write-Host "✓ Qt6 found in standard location" -ForegroundColor Green
    $qtFound = $true
} elseif ($env:CMAKE_PREFIX_PATH -like "*Qt*6*") {
    Write-Host "✓ Qt6 found in CMAKE_PREFIX_PATH" -ForegroundColor Green
    $qtFound = $true
} else {
    Write-Host "⚠ Qt6 not detected in standard locations" -ForegroundColor Yellow
    Write-Host "  Install Qt6 from https://www.qt.io/download" -ForegroundColor Yellow
    Write-Host "  Then set CMAKE_PREFIX_PATH environment variable to your Qt6 install:" -ForegroundColor Yellow
    Write-Host "  e.g. C:\Qt\6.7.0\msvc2022_64" -ForegroundColor Yellow
}

# Create third_party directory
Write-Host "Creating third_party directory structure..." -ForegroundColor Yellow
$thirdPartyPath = "third_party/discord_social_sdk"
if (-not (Test-Path $thirdPartyPath)) {
    New-Item -ItemType Directory -Path $thirdPartyPath -Force | Out-Null
    Write-Host "✓ Created $thirdPartyPath" -ForegroundColor Green
} else {
    Write-Host "✓ $thirdPartyPath already exists" -ForegroundColor Green
}

Write-Host ""
Write-Host "Discord Social SDK Setup" -ForegroundColor Yellow
Write-Host "Visit: https://docs.discord.com/developers/discord-social-sdk/getting-started/using-c%2B%2B" -ForegroundColor Cyan
Write-Host "Download the SDK and extract it to: $(Get-Location)\$thirdPartyPath" -ForegroundColor Cyan
Write-Host ""

# Copy example config
Write-Host "Setting up config..." -ForegroundColor Yellow
if (-not (Test-Path "config/omnipresence.json")) {
    Copy-Item "config/omnipresence.example.json" "config/omnipresence.json"
    Write-Host "✓ Copied omnipresence.example.json to omnipresence.json" -ForegroundColor Green
} else {
    Write-Host "✓ config/omnipresence.json already exists" -ForegroundColor Green
}

Write-Host ""
Write-Host "Setup complete! Next steps:" -ForegroundColor Cyan
Write-Host "1. Download the Discord Social SDK to: $(Get-Location)\$thirdPartyPath" -ForegroundColor Cyan
Write-Host "2. Review and customize: config/omnipresence.json" -ForegroundColor Cyan
Write-Host "3. Run: .\scripts\build-windows.ps1" -ForegroundColor Cyan
