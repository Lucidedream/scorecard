add_executable(GolfStatsTest
  GolfStatsTest.cpp
  ${REPO_ROOT}/src/golf/GolfStats.cpp
)

target_compile_definitions(GolfStatsTest PRIVATE CROSSPOINT_GOLF=1)
target_include_directories(GolfStatsTest PRIVATE ${REPO_ROOT}/src/golf)
target_link_libraries(GolfStatsTest PRIVATE crosspoint_test_common GTest::gtest_main)

gtest_discover_tests(GolfStatsTest)
