if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

file(GLOB_RECURSE FORGEDB_TEST_SOURCES CONFIGURE_DEPENDS *.cpp)
add_executable(forgedb_tests ${FORGEDB_TEST_SOURCES})
target_link_libraries(forgedb_tests PRIVATE forgedb GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(forgedb_tests)
