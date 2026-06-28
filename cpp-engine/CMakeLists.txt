cmake_minimum_required(VERSION 3.20)

project(StochEdgeCppEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(stochedge_engine
    src/main.cpp
)

if (MSVC)
    target_compile_options(stochedge_engine PRIVATE /W4 /permissive-)
else()
    target_compile_options(stochedge_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()
