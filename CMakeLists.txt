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

# ! PGO actually slowdowns executable by 200 ms. Do not use.s
#target_compile_options("${PROJECT_NAME}" PRIVATE -fprofile-instr-generate)
#target_link_options("${PROJECT_NAME}" PRIVATE -fprofile-instr-generate)
# for ((i=0; i < 100; ++i)) ; do LLVM_PROFILE_FILE="$i.profraw" ./freq pg.txt out.txt && md5sum out.txt reference.txt ; done
# llvm-profdata merge -output=freq.profdata *.profraw
#target_compile_options("${PROJECT_NAME}" PRIVATE "-fprofile-instr-use=${CMAKE_CURRENT_SOURCE_DIR}/freq.profdata")
#target_link_options("${PROJECT_NAME}" PRIVATE "-fprofile-instr-use=${CMAKE_CURRENT_SOURCE_DIR}/freq.profdata")
