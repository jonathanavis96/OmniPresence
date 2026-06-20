#!/usr/bin/env bash
set -euo pipefail

echo "OmniPresence Dev Setup"
echo ""

# Check for CMake
echo "Checking for CMake..."
if command -v cmake &> /dev/null; then
    cmake_version=$(cmake --version | head -n 1)
    echo "✓ CMake found: $cmake_version"
else
    echo "✗ CMake not found. Install via: apt-get install cmake (Debian/Ubuntu) or brew install cmake (macOS)"
    exit 1
fi

# Check for C++ compiler
echo "Checking for C++ compiler..."
if command -v g++ &> /dev/null; then
    gxx_version=$(g++ --version | head -n 1)
    echo "✓ g++ found: $gxx_version"
else
    echo "✗ g++ not found. Install via: apt-get install build-essential (Debian/Ubuntu) or brew install gcc (macOS)"
    exit 1
fi

echo ""
echo "⚠ Note: Full OmniPresence build requires:"
echo "  - Qt6 (Windows/macOS/Linux)"
echo "  - Discord Social SDK"
echo "  - Visual Studio or MSVC (for Windows build)"
echo "  This script sets up the development environment, but the app itself is Windows-only."
echo ""

# Create third_party directory
echo "Creating third_party directory structure..."
mkdir -p third_party/discord_social_sdk
echo "✓ Created third_party/discord_social_sdk"

# Copy example config
echo "Setting up config..."
if [ ! -f "config/omnipresence.json" ]; then
    cp config/omnipresence.example.json config/omnipresence.json
    echo "✓ Copied omnipresence.example.json to omnipresence.json"
else
    echo "✓ config/omnipresence.json already exists"
fi

echo ""
echo "Setup complete! Next steps:"
echo "1. Install Qt6 (if building locally)"
echo "2. Download Discord Social SDK to: $(pwd)/third_party/discord_social_sdk"
echo "   See: https://docs.discord.com/developers/discord-social-sdk/getting-started/using-c%2B%2B"
echo "3. Review and customize: config/omnipresence.json"
echo "4. For Windows build, use: scripts/build-windows.ps1"
