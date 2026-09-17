cmake_minimum_required(VERSION 3.20)

project(
    ForgeDB
    VERSION 0.1.0
    DESCRIPTION "A disk-backed embedded database engine built from scratch in C++"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Compiler warnings
# ---------------------------------------------------------------------------

if(MSVC)
    add_compile_options(
        /W4
        /permissive-
    )
else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
    )
endif()

# ---------------------------------------------------------------------------
# ForgeDB library
# ---------------------------------------------------------------------------

file(GLOB_RECURSE FORGEDB_SOURCES
    CONFIGURE_DEPENDS
    src/*.cpp
)

add_library(forgedb STATIC ${FORGEDB_SOURCES})

target_include_directories(forgedb
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

option(FORGEDB_BUILD_TESTS "Build ForgeDB tests" ON)

if(FORGEDB_BUILD_TESTS)
    enable_testing()

    add_subdirectory(tests)
endif()