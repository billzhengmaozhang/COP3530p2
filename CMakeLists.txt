cmake_minimum_required(VERSION 3.16)
project(MiniDB VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#shared include path for all targets.
add_library(minidb_headers INTERFACE)
target_include_directories(minidb_headers INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/minidb"
)
target_compile_features(minidb_headers INTERFACE cxx_std_17)

#Compile each header independently, this should catch any missing includes in the scaffold
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
if(MSVC)
    target_compile_options(minidb_lib PRIVATE /W4)
else()
    target_compile_options(minidb_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(minidb_scaffold OBJECT ${MINIDB_HEADER_CHECKS})
target_link_libraries(minidb_scaffold PRIVATE minidb_headers)

#Main CLI executable
add_executable(minidb_cli src/main.cpp)
target_link_libraries(minidb_cli PRIVATE minidb_lib)

#Catch2 and CTest targets
include(CTest)
if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG        v3.5.4
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(minidb_tests
            tests/BPlusTreeIndexTests.cpp
            # everyone add your tests here as we make them
    )
    target_link_libraries(minidb_tests PRIVATE minidb_lib Catch2::Catch2WithMain)

    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    include(Catch)
    catch_discover_tests(minidb_tests)
endif()
