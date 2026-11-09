cmake_minimum_required(VERSION 3.16)
project(MiniDB VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Benchmark timings are only meaningful for optimized builds.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(MINIDB_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(MINIDB_WERROR "Treat compiler warnings as errors" OFF)

#shared include path for all targets.
add_library(minidb_headers INTERFACE)
target_include_directories(minidb_headers INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_features(minidb_headers INTERFACE cxx_std_17)

#Compile each header independently to catch missing includes (built only with the "headers" target)
set(MINIDB_HEADER_NAMES
        Types Error RecordValidator IIndex RecordStore HashIndex
        BPlusTreeIndex Database CsvIO DatasetGenerator Benchmark Cli
)
set(MINIDB_HEADER_CHECKS)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/header_checks")
foreach(name IN LISTS MINIDB_HEADER_NAMES)
    set(check_file "${CMAKE_CURRENT_BINARY_DIR}/header_checks/${name}.cpp")
    file(WRITE "${check_file}" "#include \"minidb/${name}.h\"\n")
    list(APPEND MINIDB_HEADER_CHECKS "${check_file}")
endforeach()

#Core library target
add_library(minidb_lib
        src/Error.cpp
        src/RecordValidator.cpp
        src/IIndex.cpp
        src/RecordStore.cpp
        src/HashIndex.cpp
        src/BPlusTreeIndex.cpp
        src/Database.cpp
        src/CsvIO.cpp
        src/DatasetGenerator.cpp
        src/Benchmark.cpp
        src/Cli.cpp
)
target_link_libraries(minidb_lib PUBLIC minidb_headers)
set_target_properties(minidb_lib PROPERTIES CXX_EXTENSIONS OFF)

add_library(minidb_scaffold OBJECT EXCLUDE_FROM_ALL ${MINIDB_HEADER_CHECKS})
target_link_libraries(minidb_scaffold PRIVATE minidb_headers)
add_custom_target(headers DEPENDS minidb_scaffold)

# Warnings and sanitizers apply to the library, CLI and tests alike.
function(minidb_configure_target target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(MINIDB_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
        if(MINIDB_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
        if(MINIDB_SANITIZE)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()
minidb_configure_target(minidb_lib)

#Main CLI executable
add_executable(minidb_cli src/main.cpp)
target_link_libraries(minidb_cli PRIVATE minidb_lib)
minidb_configure_target(minidb_cli)

#Catch2 and CTest targets
include(CTest)
if(BUILD_TESTING)
    find_package(Catch2 3 QUIET)
    if(NOT Catch2_FOUND)
        include(FetchContent)
        FetchContent_Declare(
                Catch2
                GIT_REPOSITORY https://github.com/catchorg/Catch2.git
                GIT_TAG        v3.5.4
        )
        FetchContent_MakeAvailable(Catch2)
        list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    endif()

    add_executable(minidb_tests
            tests/BPlusTreeIndexTests.cpp
            tests/HashIndexTests.cpp
            tests/IndexDifferentialTests.cpp
            tests/RecordValidatorTests.cpp
            tests/CsvIOTests.cpp
            tests/DatabaseTests.cpp
            tests/DatasetGeneratorTests.cpp
            tests/BenchmarkAndCliTests.cpp
    )
    target_link_libraries(minidb_tests PRIVATE minidb_lib Catch2::Catch2WithMain)
    minidb_configure_target(minidb_tests)

    include(Catch)
    catch_discover_tests(minidb_tests)
endif()
