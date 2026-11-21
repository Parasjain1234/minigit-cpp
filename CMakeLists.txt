cmake_minimum_required(VERSION 3.16)

project(minigit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MINIGIT_BUILD_TESTS "Build the unit test suite" ON)
option(MINIGIT_BUILD_BENCH "Build the benchmark executable" ON)
option(MINIGIT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_library(minigit_core STATIC
    src/compress/huffman.cpp
    src/diff/diff_types.cpp
    src/diff/lcs.cpp
    src/diff/myers.cpp
    src/diff/unified.cpp
    src/hash/sha1.cpp
    src/merge/lca.cpp
    src/merge/three_way.cpp
    src/objects/oid.cpp
    src/objects/object.cpp
    src/objects/object_store.cpp
    src/objects/tree.cpp
    src/objects/commit.cpp
    src/repo/history.cpp
    src/repo/ignore.cpp
    src/repo/index.cpp
    src/repo/repository.cpp
    src/cli/graph.cpp
)

target_include_directories(minigit_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MSVC)
    target_compile_options(minigit_core PRIVATE /W4)
    if(MINIGIT_WARNINGS_AS_ERRORS)
        target_compile_options(minigit_core PRIVATE /WX)
    endif()
else()
    target_compile_options(minigit_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
    if(MINIGIT_WARNINGS_AS_ERRORS)
        target_compile_options(minigit_core PRIVATE -Werror)
    endif()
endif()

add_executable(minigit src/cli/main.cpp)
target_link_libraries(minigit PRIVATE minigit_core)

if(MINIGIT_BUILD_BENCH)
    add_executable(minigit_bench bench/bench.cpp)
    target_link_libraries(minigit_bench PRIVATE minigit_core)
endif()

if(MINIGIT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
