cmake_minimum_required(VERSION 3.20)

project(
    ForgeDB
    VERSION 1.0.0
    DESCRIPTION "A disk-backed embedded database engine built from scratch in C++20"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /EHsc)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

# ------------------------------------------------------------
# ForgeDB Library
# ------------------------------------------------------------

file(GLOB_RECURSE FORGEDB_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
)

add_library(forgedb STATIC ${FORGEDB_SOURCES})

target_include_directories(forgedb
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

# ------------------------------------------------------------
# Examples
# ------------------------------------------------------------

option(FORGEDB_BUILD_EXAMPLES "Build ForgeDB examples" ON)

if(FORGEDB_BUILD_EXAMPLES)

    add_executable(
        forgedb_basic_example
        examples/basic_database.cpp
    )

    target_link_libraries(
        forgedb_basic_example
        PRIVATE forgedb
    )

    add_executable(
        forgedb_persistent_index_example
        examples/persistent_index.cpp
    )

    target_link_libraries(
        forgedb_persistent_index_example
        PRIVATE forgedb
    )

endif()

# ------------------------------------------------------------
# Benchmarks
# ------------------------------------------------------------

option(FORGEDB_BUILD_BENCHMARKS "Build ForgeDB benchmarks" ON)

if(FORGEDB_BUILD_BENCHMARKS)

    add_executable(
        forgedb_benchmark
        benchmarks/forgedb_benchmark.cpp
    )

    target_link_libraries(
        forgedb_benchmark
        PRIVATE forgedb
    )

endif()

# ------------------------------------------------------------
# Installation
# ------------------------------------------------------------

include(GNUInstallDirs)

install(
    TARGETS forgedb
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
# ------------------------------------------------------------
# Regression tests
# ------------------------------------------------------------

include(CTest)

if(BUILD_TESTING)
    add_executable(
        forgedb_dirty_page_eviction_regression
        tests/dirty_page_eviction_regression.cpp
    )

    target_link_libraries(
        forgedb_dirty_page_eviction_regression
        PRIVATE forgedb
    )

    add_test(
        NAME dirty_page_eviction_persistence
        COMMAND forgedb_dirty_page_eviction_regression
    )
endif()

if(BUILD_TESTING)
    add_executable(
        forgedb_heap_free_space_index_regression
        tests/heap_free_space_index_regression.cpp
    )
    target_link_libraries(
        forgedb_heap_free_space_index_regression
        PRIVATE forgedb
    )
    add_test(
        NAME heap_free_space_index_variable_rows
        COMMAND forgedb_heap_free_space_index_regression
    )

    add_executable(
        forgedb_heap_metadata_directory_regression
        tests/heap_metadata_directory_regression.cpp
    )

    target_link_libraries(
        forgedb_heap_metadata_directory_regression
        PRIVATE forgedb
    )

    add_test(
        NAME heap_metadata_directory_overflow
        COMMAND forgedb_heap_metadata_directory_regression
    )
endif()

if(BUILD_TESTING)
    add_executable(
        forgedb_persistent_b_plus_tree_insert_regression
        tests/persistent_b_plus_tree_insert_regression.cpp
    )
    target_link_libraries(
        forgedb_persistent_b_plus_tree_insert_regression
        PRIVATE forgedb
    )
    add_test(
        NAME persistent_b_plus_tree_insert_reopen
        COMMAND forgedb_persistent_b_plus_tree_insert_regression
    )
endif()

if(BUILD_TESTING)
    add_executable(
        forgedb_persistent_b_plus_tree_range_scan_regression
        tests/persistent_b_plus_tree_range_scan_regression.cpp
    )
    target_link_libraries(
        forgedb_persistent_b_plus_tree_range_scan_regression
        PRIVATE forgedb
    )
    add_test(
        NAME persistent_b_plus_tree_range_scan
        COMMAND forgedb_persistent_b_plus_tree_range_scan_regression
    )
endif()

if(BUILD_TESTING)
    add_executable(
        forgedb_heap_scan_batch_regression
        tests/heap_scan_batch_regression.cpp
    )
    target_link_libraries(
        forgedb_heap_scan_batch_regression
        PRIVATE forgedb
    )
    add_test(
        NAME heap_batched_scan_deleted_rows_reopen
        COMMAND forgedb_heap_scan_batch_regression
    )
endif()
