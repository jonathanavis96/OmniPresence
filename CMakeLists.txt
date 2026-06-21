# OmniPresence — top-level CMakeLists.txt
# Targets Windows 11 + Qt 6. Qt6 is NOT expected to be present on the build
# host (WSL/Linux dev box); this file is authoritative for Windows builds.
cmake_minimum_required(VERSION 3.21)

project(OmniPresence VERSION 0.1.0 LANGUAGES CXX C)

# ── C++ standard ──────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Feature options ────────────────────────────────────────────────────────────
# Discord Social SDK lives in third_party/discord_social_sdk/ (gitignored).
# Run scripts/setup-dev to download it before enabling this flag.
option(OMNIPRESENCE_WITH_DISCORD "Link the Discord Social SDK" OFF)
option(OMNIPRESENCE_WITH_QT     "Build the Qt GUI"             ON)

# ── Global compile definitions passed down to all sub-targets ─────────────────
if(OMNIPRESENCE_WITH_DISCORD)
    add_compile_definitions(OMNIPRESENCE_WITH_DISCORD)
endif()

# ── Sub-directories ────────────────────────────────────────────────────────────
add_subdirectory(app)

# ── Dev tooling (optional) ────────────────────────────────────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/CMakeLists.txt")
    add_subdirectory(scripts)
endif()

# ── Tests (optional – only wired when the sub-dir exists) ─────────────────────
enable_testing()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
