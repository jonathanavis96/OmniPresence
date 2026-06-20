$ErrorActionPreference = 'Stop'

Write-Host "OmniPresence Windows Build" -ForegroundColor Cyan
Write-Host ""

# Check if config exists
if (-not (Test-Path "config/omnipresence.json")) {
    Write-Host "✗ config/omnipresence.json not found. Run setup-dev.ps1 first." -ForegroundColor Red
    exit 1
}

# Determine Qt6 path and CMAKE_PREFIX_PATH
$qtPrefix = ""
if ($env:CMAKE_PREFIX_PATH -like "*Qt*") {
    $qtPrefix = $env:CMAKE_PREFIX_PATH
    Write-Host "Using CMAKE_PREFIX_PATH: $qtPrefix" -ForegroundColor Yellow
} else {
    # Try standard Qt location
    $stdQtPath = Get-ChildItem "C:\Qt\6*" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($stdQtPath) {
        $qtPrefix = Join-Path $stdQtPath.FullName "msvc2022_64"
        if (-not (Test-Path $qtPrefix)) {
            $qtPrefix = $stdQtPath.FullName
        }
        Write-Host "Found Qt6 at: $qtPrefix" -ForegroundColor Yellow
    }
}

# Build flags
$cmakeFlags = @("-S", ".", "-B", "build", "-DCMAKE_BUILD_TYPE=Release")

if ($qtPrefix) {
    $cmakeFlags += "-DCMAKE_PREFIX_PATH=$qtPrefix"
}

# Check for Discord SDK
if (Test-Path "third_party/discord_social_sdk" -PathType Container) {
    $sdkFiles = @(Get-ChildItem "third_party/discord_social_sdk" -Recurse -ErrorAction SilentlyContinue)
    if ($sdkFiles.Count -gt 0) {
        $cmakeFlags += "-DOMNIPRESENCE_WITH_DISCORD=ON"
        Write-Host "Discord Social SDK found, enabling Discord support" -ForegroundColor Green
    } else {
        Write-Host "⚠ discord_social_sdk folder is empty. Discord support will be disabled." -ForegroundColor Yellow
    }
} else {
    Write-Host "⚠ discord_social_sdk not found. Discord support will be disabled." -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Running CMake configuration..." -ForegroundColor Cyan
Write-Host "cmake $($cmakeFlags -join ' ')" -ForegroundColor Gray
cmake @cmakeFlags

Write-Host ""
Write-Host "Building release binaries..." -ForegroundColor Cyan
cmake --build build --config Release

Write-Host ""
Write-Host "Build complete!" -ForegroundColor Green
Write-Host "Executable location: $(Get-Location)\build\app\Release\omnipresence.exe" -ForegroundColor Green
Write-Host ""
