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

# Headers MUST be listed as target sources so AUTOMOC scans the Q_OBJECT classes
# and generates the moc_*.cpp (otherwise: unresolved staticMetaObject / signal
# symbols at link time for classes whose headers live in a separate include dir).
set(OMNIPRESENCE_HEADERS
    include/WindowInfo.h
    include/ActiveWindowWatcher.h
    include/StubActiveWindowWatcher.h
    include/PresencePayload.h
    include/FocusDebouncer.h
    include/Rule.h
    include/IntegrationContext.h
    include/TemplateEngine.h
    include/RuleEngine.h
    include/DiscordPresenceClient.h
    include/LocalContextServer.h
    include/ConfigStore.h
    include/AppController.h
)
list(APPEND OMNIPRESENCE_SOURCES ${OMNIPRESENCE_HEADERS})

# Win32-only active window watcher — compiled only on Windows
if(WIN32)
    list(APPEND OMNIPRESENCE_SOURCES
        src/Win32ActiveWindowWatcher.cpp
        include/Win32ActiveWindowWatcher.h)
endif()

# ── Executable ────────────────────────────────────────────────────────────────
qt_add_executable(omnipresence
    ${OMNIPRESENCE_SOURCES}
)

# No console window on Windows release builds
set_target_properties(omnipresence PROPERTIES
    WIN32_EXECUTABLE ON
)

# Win32ActiveWindowWatcher's Q_OBJECT class is guarded by #ifdef _WIN32. moc does
# not define _WIN32 itself, so without this it skips the class and the meta-object
# symbols go unresolved at link. Define it for moc on Windows builds.
if(WIN32)
    set_target_properties(omnipresence PROPERTIES AUTOMOC_MOC_OPTIONS "-D_WIN32")
endif()

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

    # SDK layout (v1.9): the import lib is discord_partner_sdk.lib under
    # lib/<config>/ and the runtime DLL is bin/<config>/discord_partner_sdk.dll.
    set(DISCORD_LIB_DIR "${DISCORD_SDK_ROOT}/lib/$<IF:$<CONFIG:Debug>,debug,release>")
    set(DISCORD_BIN_DIR "${DISCORD_SDK_ROOT}/bin/$<IF:$<CONFIG:Debug>,debug,release>")

    target_include_directories(omnipresence PRIVATE "${DISCORD_SDK_ROOT}/include")
    target_link_directories(omnipresence PRIVATE "${DISCORD_LIB_DIR}")
    target_link_libraries(omnipresence PRIVATE discord_partner_sdk)

    # discordpp.h is a single-header lib: its method bodies compile only in the
    # one TU that defines DISCORDPP_IMPLEMENTATION (see discord_sdk_impl.cpp).
    target_sources(omnipresence PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/discord_sdk_impl.cpp")

    # Copy the SDK DLL next to the executable so it loads at runtime.
    add_custom_command(TARGET omnipresence POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DISCORD_BIN_DIR}/discord_partner_sdk.dll"
            "$<TARGET_FILE_DIR:omnipresence>"
        COMMENT "Copying discord_partner_sdk.dll next to omnipresence.exe")
endif()

# ── Compiler warnings ─────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(omnipresence PRIVATE /W4 /permissive-)
else()
    target_compile_options(omnipresence PRIVATE -Wall -Wextra -Wpedantic)
endif()
