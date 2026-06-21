# tests/CMakeLists.txt — QtTest harness for pure-logic units.
# Each test file is its own executable (one QTEST_MAIN per exe).
find_package(Qt6 REQUIRED COMPONENTS Test Core Gui)

set(CMAKE_AUTOMOC ON)
set(OMNI_TEST_SRC ${CMAKE_SOURCE_DIR}/app/src)

add_executable(test_template_engine
    test_template_engine.cpp
    ${OMNI_TEST_SRC}/TemplateEngine.cpp
    ${OMNI_TEST_SRC}/IntegrationContext.cpp
)
target_include_directories(test_template_engine PRIVATE ${CMAKE_SOURCE_DIR}/app/include)
target_link_libraries(test_template_engine PRIVATE Qt6::Test Qt6::Core Qt6::Gui)
add_test(NAME test_template_engine COMMAND test_template_engine)

add_executable(test_art_store
    test_art_store.cpp
    ${OMNI_TEST_SRC}/ArtStore.cpp
)
target_include_directories(test_art_store PRIVATE ${CMAKE_SOURCE_DIR}/app/include)
target_link_libraries(test_art_store PRIVATE Qt6::Test Qt6::Core Qt6::Gui)
add_test(NAME test_art_store COMMAND test_art_store)

add_executable(test_config_assets
    test_config_assets.cpp
    ${OMNI_TEST_SRC}/ConfigStore.cpp
    ${OMNI_TEST_SRC}/Rule.cpp
    ${CMAKE_SOURCE_DIR}/app/include/ConfigStore.h   # AUTOMOC scans Q_OBJECT header
)
target_include_directories(test_config_assets PRIVATE ${CMAKE_SOURCE_DIR}/app/include)
target_link_libraries(test_config_assets PRIVATE Qt6::Test Qt6::Core Qt6::Gui)
add_test(NAME test_config_assets COMMAND test_config_assets)

add_executable(test_rule_engine_render
    test_rule_engine_render.cpp
    ${OMNI_TEST_SRC}/RuleEngine.cpp
    ${OMNI_TEST_SRC}/TemplateEngine.cpp
    ${OMNI_TEST_SRC}/IntegrationContext.cpp
    ${OMNI_TEST_SRC}/Rule.cpp
    ${OMNI_TEST_SRC}/PresencePayload.cpp
)
target_include_directories(test_rule_engine_render PRIVATE ${CMAKE_SOURCE_DIR}/app/include)
target_link_libraries(test_rule_engine_render PRIVATE Qt6::Test Qt6::Core Qt6::Gui)
add_test(NAME test_rule_engine_render COMMAND test_rule_engine_render)
