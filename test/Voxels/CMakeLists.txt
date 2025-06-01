file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(unit_tests ${TEST_SOURCES})

target_link_libraries(unit_tests
    PRIVATE
    VoxelsLib
    GTest::gtest_main
)

target_include_directories(unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

include(GoogleTest)
gtest_discover_tests(unit_tests)
