# scripts/CMakeLists.txt — dev tooling, not part of the app.
find_package(Qt6 REQUIRED COMPONENTS Gui)

add_executable(make-placeholder-art
    make-placeholder-art.cpp
    ${CMAKE_SOURCE_DIR}/app/src/ArtStore.cpp)
target_include_directories(make-placeholder-art PRIVATE ${CMAKE_SOURCE_DIR}/app/include)
target_link_libraries(make-placeholder-art PRIVATE Qt6::Gui)
