# OmniPresence — app/CMakeLists.txt
# Builds the Qt6 GUI executable and wires platform-specific sources.

if(NOT OMNIPRESENCE_WITH_QT)
    message(STATUS "OMNIPRESENCE_WITH_QT is OFF — skipping Qt GUI build")
    return()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network Widgets)
qt_standard_project_setup()

# ── Source files ──────────────────────────────────────────────────────────────
set(OMNIPRESENCE_SOURCES
    src/main.cpp
    src/ActiveWindowWatcher.cpp
    src/StubActiveWindowWatcher.cpp
    src/PresencePayload.cpp
    src/FocusDebouncer.cpp
    src/Rule.cpp
    src/IntegrationContext.cpp
    src/TemplateEngine.cpp
    src/RuleEngine.cpp
    src/DiscordPresenceClient.cpp
    src/LocalContextServer.cpp
    src/ConfigStore.cpp
    src/AppController.cpp
)

# Win32-only active window watcher — compiled only on Windows
if(WIN32)
    list(APPEND OMNIPRESENCE_SOURCES src/Win32ActiveWindowWatcher.cpp)
endif()

# ── Executable ────────────────────────────────────────────────────────────────
qt_add_executable(omnipresence
    ${OMNIPRESENCE_SOURCES}
)

# No console window on Windows release builds
set_target_properties(omnipresence PROPERTIES
    WIN32_EXECUTABLE ON
)

# ── Include directories ───────────────────────────────────────────────────────
target_include_directories(omnipresence PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ── QML module ────────────────────────────────────────────────────────────────
qt_add_qml_module(omnipresence
    URI OmniPresence
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Dashboard.qml
        qml/CapturePage.qml
        qml/RulesPage.qml
        qml/PreviewPage.qml
        qml/PrivacyPage.qml
        qml/AssetManager.qml
    RESOURCES
        resources/README.md
)

# ── Link libraries ────────────────────────────────────────────────────────────
target_link_libraries(omnipresence PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    Qt6::Widgets
)

if(WIN32)
    target_link_libraries(omnipresence PRIVATE user32)
endif()

# ── Discord Social SDK (optional) ─────────────────────────────────────────────
# Download the SDK first: scripts/setup-dev (see docs/development-setup.md)
# SDK expected at: third_party/discord_social_sdk/
if(OMNIPRESENCE_WITH_DISCORD)
    set(DISCORD_SDK_ROOT "${CMAKE_SOURCE_DIR}/third_party/discord_social_sdk")
    if(NOT EXISTS "${DISCORD_SDK_ROOT}/include/discordpp.h")
        message(FATAL_ERROR
            "Discord Social SDK not found at ${DISCORD_SDK_ROOT}. "
            "Run scripts/setup-dev or set DISCORD_SDK_ROOT manually.")
    endif()
    target_include_directories(omnipresence PRIVATE "${DISCORD_SDK_ROOT}/include")
    target_link_directories(omnipresence PRIVATE "${DISCORD_SDK_ROOT}/lib")
    target_link_libraries(omnipresence PRIVATE discord_social_sdk)  # TODO: verify exact lib name
endif()

# ── Compiler warnings ─────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(omnipresence PRIVATE /W4 /permissive-)
else()
    target_compile_options(omnipresence PRIVATE -Wall -Wextra -Wpedantic)
endif()
