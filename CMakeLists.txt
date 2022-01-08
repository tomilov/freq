cmake_minimum_required(VERSION 3.12)
project("freq")

include(CheckIncludeFileCXX)

find_package(OpenMP)
find_package(absl)
find_package(folly)
find_package(gflags)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS ON)

add_library("libc++" INTERFACE)
target_compile_options("libc++" INTERFACE $<$<CXX_COMPILER_ID:Clang>:-stdlib=libc++>)
target_link_options("libc++" INTERFACE $<$<CXX_COMPILER_ID:Clang>:-stdlib=libc++>)

add_library("libstdc++" INTERFACE)
target_compile_options("libstdc++" INTERFACE $<$<CXX_COMPILER_ID:Clang>:-stdlib=libstdc++>)
target_link_options("libstdc++" INTERFACE $<$<CXX_COMPILER_ID:Clang>:-stdlib=libstdc++>)

add_executable("unordered_map")
target_sources(
    "unordered_map"
    PRIVATE
        "unordered_map.cpp"
        "basic.hpp"
        "timer.hpp"
        "helpers.hpp")
target_link_libraries("unordered_map" PRIVATE "libc++")

add_executable("unordered_map_libstdc++")
target_sources(
    "unordered_map_libstdc++"
    PRIVATE
        "unordered_map.cpp"
        "basic.hpp"
        "timer.hpp"
        "helpers.hpp")
target_link_libraries("unordered_map_libstdc++" PRIVATE "libstdc++")

CHECK_INCLUDE_FILE_CXX("google/dense_hash_map" google_dense_hash_map_FOUND)
CHECK_INCLUDE_FILE_CXX("sparse_hash_map/dense_hash_map" sparse_hash_map_dense_hash_map_FOUND)
if(google_dense_hash_map_FOUND OR sparse_hash_map_dense_hash_map_FOUND)
    add_executable("dense_hash_map")
    target_sources(
        "dense_hash_map"
        PRIVATE
            "dense_hash_map.cpp"
            "basic.hpp"
            "timer.hpp"
            "helpers.hpp")
    target_link_libraries("dense_hash_map" PRIVATE "libc++")
else()
    message(STATUS "Target dense_hash_map disabled")
endif()

CHECK_INCLUDE_FILE_CXX("google/sparse_hash_map" google_sparse_hash_map_FOUND)
CHECK_INCLUDE_FILE_CXX("sparse_hash_map/sparse_hash_map" sparse_hash_map_sparse_hash_map_FOUND)
if(google_sparse_hash_map_FOUND OR sparse_hash_map_sparse_hash_map_FOUND)
    add_executable("sparse_hash_map")
    target_sources(
        "sparse_hash_map"
        PRIVATE
            "sparse_hash_map.cpp"
            "basic.hpp"
            "timer.hpp"
            "helpers.hpp")
    target_link_libraries("sparse_hash_map" PRIVATE "libc++")
else()
    message(STATUS "Target sparse_hash_map disabled")
endif()

if(TARGET Folly::folly AND TARGET gflags)
    add_executable("folly")
    target_sources(
        "folly"
        PRIVATE
            "folly.cpp"
            "basic.hpp"
            "timer.hpp"
            "helpers.hpp")
    target_link_libraries(
        "folly"
        PRIVATE
            "libstdc++"
            Folly::folly
            gflags)
else()
    message(STATUS "Target folly disabled")
endif()

if(TARGET absl::flat_hash_map)
    add_executable("absl")
    target_sources(
        "absl"
        PRIVATE
            "absl.cpp"
            "basic.hpp"
            "timer.hpp"
            "helpers.hpp")
    target_link_libraries(
        "absl"
        PRIVATE
            "libc++"
            absl::flat_hash_map)
else()
    message(STATUS "Target absl disabled")
endif()

add_executable("pb_ds")
target_sources(
    "pb_ds"
    PRIVATE
        "pb_ds.cpp"
        "basic.hpp"
        "timer.hpp"
        "helpers.hpp")
target_link_libraries("pb_ds" PRIVATE "libstdc++")

add_executable("trie")
target_sources(
    "trie"
    PRIVATE
        "trie.cpp"
        "io.hpp"
        "timer.hpp"
        "helpers.hpp")
target_link_libraries("trie" PRIVATE "libc++")

add_executable("oaph")
target_sources(
    "oaph"
    PRIVATE
        "oaph.cpp"
        "io.hpp"
        "timer.hpp"
        "helpers.hpp")
target_link_libraries(
    "oaph"
    PRIVATE
        "libc++"
        $<$<BOOL:OpenMP_CXX_FOUND>:OpenMP::OpenMP_CXX>)

