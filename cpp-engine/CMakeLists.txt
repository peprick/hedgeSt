cmake_minimum_required(VERSION 3.20)

project(StochHedgeCppEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(stoch_hedge_first_step
    src/main.cpp
)

if (MSVC)
    target_compile_options(stoch_hedge_first_step PRIVATE /W4 /permissive-)
else()
    target_compile_options(stoch_hedge_first_step PRIVATE -Wall -Wextra -Wpedantic)
endif()
