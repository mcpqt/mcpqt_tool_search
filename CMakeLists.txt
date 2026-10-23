cmake_minimum_required(VERSION 3.20)
project(mcpqt_tool_search VERSION 0.1.0 LANGUAGES CXX)

# ----- C++ / Qt boilerplate ------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Plugins are QObject-derived Qt plugins, so moc must run.
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core)


add_library(mcpqt_tool_search MODULE
    src/tool_search_plugin.cpp
    src/plugin.cpp
    src/bm25.cpp
    src/bm25.h
)
set_target_properties(mcpqt_tool_search PROPERTIES PREFIX "")
target_include_directories(mcpqt_tool_search PRIVATE src)
target_link_libraries(mcpqt_tool_search PRIVATE Qt6::Core)


target_compile_options(mcpqt_tool_search PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
        -Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)
