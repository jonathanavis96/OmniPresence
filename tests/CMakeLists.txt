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
