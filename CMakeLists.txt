cmake_minimum_required(VERSION 3.16)
project(MiniDB VERSION 0.1.0 LANGUAGES CXX)

# Shared include path and language requirement for future implementation targets.
add_library(minidb_headers INTERFACE)
target_include_directories(minidb_headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(minidb_headers INTERFACE cxx_std_17)

# Compile each header independently, to catch missing includes in the scaffold.
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

# Objects only: declarations are not linked into a working database application.
add_library(minidb_scaffold OBJECT
    ${MINIDB_HEADER_CHECKS}
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
    src/main.cpp
)
target_link_libraries(minidb_scaffold PRIVATE minidb_headers)
set_target_properties(minidb_scaffold PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(minidb_scaffold PRIVATE /W4)
else()
    target_compile_options(minidb_scaffold PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Later: add a minidb library and executable once the methods/main are defined.
# Add Catch2 and CTest when behavioral tests have been implemented.
