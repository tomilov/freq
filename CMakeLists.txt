cmake_minimum_required(VERSION 3.12.0)
project("freq")

add_executable("${PROJECT_NAME}")

target_sources(
    "${PROJECT_NAME}"
    PRIVATE
        "io.hpp"
        "timer.hpp"
        "main.cpp")

set_property(TARGET "${PROJECT_NAME}" PROPERTY CXX_STANDARD 20)
target_compile_definitions("${PROJECT_NAME}" PRIVATE $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)
target_compile_options("${PROJECT_NAME}" PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/arch:AVX,-march=native>)
