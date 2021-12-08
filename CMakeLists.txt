cmake_minimum_required(VERSION 3.12.0)
project("freq")

find_package(OpenMP REQUIRED)

add_executable("${PROJECT_NAME}")

target_sources(
    "${PROJECT_NAME}"
    PRIVATE
        "main.cpp")

target_link_libraries(
    "${PROJECT_NAME}"
    PUBLIC
        OpenMP::OpenMP_CXX)

set_property(TARGET "${PROJECT_NAME}" PROPERTY CXX_STANDARD 20)
target_compile_definitions("${PROJECT_NAME}" PRIVATE $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)
target_compile_options("${PROJECT_NAME}" PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/arch:AVX,-march=native>)

# ! PGO actually slowdowns executable by 200 ms. Do not use.
#target_compile_options("${PROJECT_NAME}" PRIVATE -fprofile-instr-generate)
#target_link_options("${PROJECT_NAME}" PRIVATE -fprofile-instr-generate)
# for ((i=0; i < 100; ++i)) ; do LLVM_PROFILE_FILE="$i.profraw" ./freq pg.txt out.txt && md5sum out.txt reference.txt ; done
# llvm-profdata merge -output=freq.profdata *.profraw
#target_compile_options("${PROJECT_NAME}" PRIVATE "-fprofile-instr-use=${CMAKE_CURRENT_SOURCE_DIR}/freq.profdata")
#target_link_options("${PROJECT_NAME}" PRIVATE "-fprofile-instr-use=${CMAKE_CURRENT_SOURCE_DIR}/freq.profdata")

#target_compile_options("${PROJECT_NAME}" PRIVATE -gline-tables-only)
#target_link_options("${PROJECT_NAME}" PRIVATE -gline-tables-only)
# find /sys/devices/system/cpu -name scaling_governor -exec sh -c 'echo performance > {}' ';'
# alias cpuisolate='sudo bash -c "echo off > /sys/devices/system/cpu/smt/control ; tuna --cpus=1-7 --isolate"'
# alias cpuinclude='sudo bash -c "echo on > /sys/devices/system/cpu/smt/control ; tuna --cpus=0-15 --include"'
# cpuisolate
# taskset --cpu-list 1-7 perf record -b ./freq /tmp/pg.txt out.txt && md5sum reference.txt out.txt
# create_llvm_prof --binary=./freq --out=freq.prof
#target_compile_options("${PROJECT_NAME}" PRIVATE -gline-tables-only -fprofile-sample-use=freq.prof)
#target_link_options("${PROJECT_NAME}" PRIVATE -gline-tables-only -fprofile-sample-use=freq.prof)
# taskset --cpu-list 1-7 ./freq /tmp/pg.txt out.txt && md5sum reference.txt out.txt
# cpuinclude
