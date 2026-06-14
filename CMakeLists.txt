cmake_minimum_required(VERSION 3.14)
project(fgrep VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-function)

set(OPT_FLAGS "-O3 -march=native -flto -funroll-loops")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${OPT_FLAGS}")

include(CheckIncludeFile)
include(CheckCCompilerFlag)

check_c_compiler_flag("-mavx2" HAS_AVX2)
check_c_compiler_flag("-mavx512f" HAS_AVX512F)
check_c_compiler_flag("-mavx512bw" HAS_AVX512BW)

add_executable(fgrep
    src/main.c
    src/simd.c
    src/cpu.c
    src/io.c
    src/fileutil.c
    src/regex_engine.c
    src/threadpool.c
    src/output.c
    src/search.c
    src/bmh_simd.c
)

target_include_directories(fgrep PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

find_package(Threads REQUIRED)
target_link_libraries(fgrep PRIVATE Threads::Threads)

install(TARGETS fgrep RUNTIME DESTINATION bin)

enable_testing()

add_executable(test_simd tests/test_simd.c src/simd.c src/cpu.c)
target_include_directories(test_simd PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_simd PRIVATE Threads::Threads)
add_test(NAME test_simd COMMAND test_simd)

add_executable(test_regex tests/test_regex.c src/regex_engine.c src/simd.c src/cpu.c)
target_include_directories(test_regex PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_regex PRIVATE Threads::Threads)
add_test(NAME test_regex COMMAND test_regex)

add_executable(test_bmh tests/test_bmh.c src/bmh_simd.c)
target_include_directories(test_bmh PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_bmh COMMAND test_bmh)

add_executable(test_integration tests/test_integration.c
    src/simd.c src/cpu.c src/io.c src/fileutil.c
    src/regex_engine.c src/threadpool.c src/output.c src/search.c
)
target_include_directories(test_integration PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_integration PRIVATE Threads::Threads)
