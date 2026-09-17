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

file(GLOB_RECURSE FORGEDB_SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(forgedb STATIC ${FORGEDB_SOURCES})

target_include_directories(forgedb PUBLIC ${PROJECT_SOURCE_DIR}/include)

option(FORGEDB_BUILD_TESTS "Build ForgeDB tests" ON)
option(FORGEDB_BUILD_EXAMPLES "Build ForgeDB examples" ON)

if(FORGEDB_BUILD_TESTS)
    enable_testing()
    find_package(GTest QUIET)
    if(GTest_FOUND)
        add_subdirectory(tests)
    else()
        include(FetchContent)
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
        add_subdirectory(tests)
    endif()
endif()

if(FORGEDB_BUILD_EXAMPLES)
    add_executable(forgedb_basic_example examples/basic_database.cpp)
    target_link_libraries(forgedb_basic_example PRIVATE forgedb)

    add_executable(forgedb_persistent_index_example examples/persistent_index.cpp)
    target_link_libraries(forgedb_persistent_index_example PRIVATE forgedb)
endif()

include(GNUInstallDirs)
install(TARGETS forgedb ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
