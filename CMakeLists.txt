cmake_minimum_required(VERSION 3.14.2)
project("freq")

add_executable("${PROJECT_NAME}" "main.cpp")
set_property(TARGET "${PROJECT_NAME}" PROPERTY CXX_STANDARD 17)
target_compile_definitions("${PROJECT_NAME}" PRIVATE $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)
target_compile_options("${PROJECT_NAME}" PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/arch:AVX,-march=native>)
