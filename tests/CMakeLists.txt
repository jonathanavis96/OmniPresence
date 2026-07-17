# tests/CMakeLists.txt — QtTest harness for pure-logic units.
# Each test file is its own executable (one QTEST_MAIN per exe).
find_package(Qt6 REQUIRED COMPONENTS Test Core Gui)

set(CMAKE_AUTOMOC ON)
set(OMNI_TEST_SRC ${CMAKE_SOURCE_DIR}/app/src)

# On Windows the test exes link Qt6Test/Core/Gui, which windeployqt does NOT
# place next to the app build. Launching a test without those DLLs on PATH pops
# a "Qt6Test.dll not found" loader dialog (one window per exe — running the
# whole suite spawned several at once). Deploy the Qt runtime next to each test
# exe as a POST_BUILD step so they are self-contained and can be double-clicked
# or run from anywhere without ever popping a dialog.
if(WIN32)
    find_program(OMNI_WINDEPLOYQT windeployqt
        HINTS "${Qt6_DIR}/../../../bin" "${Qt6Core_DIR}/../../../bin")
    function(omni_make_test_selfcontained tgt)
        if(OMNI_WINDEPLOYQT)
            add_custom_command(TARGET ${tgt} POST_BUILD
                COMMAND "${OMNI_WINDEPLOYQT}" --no-translations
                        --no-system-d3d-compiler --no-opengl-sw
                        --no-compiler-runtime "$<TARGET_FILE:${tgt}>"
                COMMENT "windeployqt ${tgt} (self-contained, no missing-DLL dialogs)"
                VERBATIM)
        endif()
    endfunction()
else()
    function(omni_make_test_selfcontained tgt)
    endfunction()
endif()

add_executable(test_template_engine
    test_template_engine.cpp
    ${OMNI_TEST_SRC}/TemplateEngine.cpp
    ${OMNI_TEST_SRC}/IntegrationContext.cpp
    ${OMNI_TEST_SRC}/SkillLabel.cpp
)
target_include_directories(test_template_engine PRIVATE ${CMAKE_SOURCE_DIR}/app/include)
target_link_libraries(test_template_engine PRIVATE Qt6::Test Qt6::Core Qt6::Gui)
add_test(NAME test_template_engine COMMAND test_template_engine)
omni_make_test_selfcontained(test_template_engine)

add_executable(test_skill_label
    test_skill_label.cpp
    ${OMNI_TEST_SRC}/SkillLabel.cpp
)
target_include_directories(test_skill_label PRIVATE ${CMAKE_SOURCE_DIR}/app/include)
target_link_libraries(test_skill_label PRIVATE Qt6::Test Qt6::Core Qt6::Gui)
add_test(NAME test_skill_label COMMAND test_skill_label)
omni_make_test_selfcontained(test_skill_label)

add_executable(test_art_store
    test_art_store.cpp
    ${OMNI_TEST_SRC}/ArtStore.cpp
)
target_include_directories(test_art_store PRIVATE ${CMAKE_SOURCE_DIR}/app/include)
target_link_libraries(test_art_store PRIVATE Qt6::Test Qt6::Core Qt6::Gui)
add_test(NAME test_art_store COMMAND test_art_store)
omni_make_test_selfcontained(test_art_store)

add_executable(test_config_assets
    test_config_assets.cpp
    ${OMNI_TEST_SRC}/ConfigStore.cpp
    ${OMNI_TEST_SRC}/Rule.cpp
    ${CMAKE_SOURCE_DIR}/app/include/ConfigStore.h   # AUTOMOC scans Q_OBJECT header
)
target_include_directories(test_config_assets PRIVATE ${CMAKE_SOURCE_DIR}/app/include)
target_link_libraries(test_config_assets PRIVATE Qt6::Test Qt6::Core Qt6::Gui)
add_test(NAME test_config_assets COMMAND test_config_assets)
omni_make_test_selfcontained(test_config_assets)

add_executable(test_rule_engine_render
    test_rule_engine_render.cpp
    ${OMNI_TEST_SRC}/RuleEngine.cpp
    ${OMNI_TEST_SRC}/TemplateEngine.cpp
    ${OMNI_TEST_SRC}/IntegrationContext.cpp
    ${OMNI_TEST_SRC}/SkillLabel.cpp
    ${OMNI_TEST_SRC}/Rule.cpp
    ${OMNI_TEST_SRC}/PresencePayload.cpp
)
target_include_directories(test_rule_engine_render PRIVATE ${CMAKE_SOURCE_DIR}/app/include)
target_link_libraries(test_rule_engine_render PRIVATE Qt6::Test Qt6::Core Qt6::Gui)
add_test(NAME test_rule_engine_render COMMAND test_rule_engine_render)
omni_make_test_selfcontained(test_rule_engine_render)
