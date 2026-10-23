cmake_minimum_required(VERSION 3.20)
project(mcpqt_example_plugin VERSION 0.1.0 LANGUAGES CXX)

# ----- C++ / Qt boilerplate ------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Plugins are QObject-derived Qt plugins, so moc must run.
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core)


add_library(mcpqt_example_plugin MODULE
    src/example_plugin.cpp
    src/plugin.cpp
    src/bm25/bm25.cpp
    src/bm25/bm25.h
)
set_target_properties(mcpqt_example_plugin PROPERTIES PREFIX "")
target_link_libraries(mcpqt_example_plugin PRIVATE Qt6::Core)


target_compile_options(mcpqt_example_plugin PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
        -Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)
