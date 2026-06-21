# scripts/CMakeLists.txt — dev tooling, not part of the app.
find_package(Qt6 REQUIRED COMPONENTS Gui)

add_executable(make-placeholder-art make-placeholder-art.cpp)
target_link_libraries(make-placeholder-art PRIVATE Qt6::Gui)
