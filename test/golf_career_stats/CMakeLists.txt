add_executable(GolfCareerStatsTest
  GolfCareerStatsTest.cpp
  ${REPO_ROOT}/src/golf/GolfCareerStats.cpp
  ${REPO_ROOT}/src/golf/GolfStats.cpp
  ${REPO_ROOT}/src/golf/GolfPenalty.cpp
  ${REPO_ROOT}/src/golf/GolfRules.cpp
)

target_compile_definitions(GolfCareerStatsTest PRIVATE CROSSPOINT_GOLF=1)
target_include_directories(GolfCareerStatsTest PRIVATE ${REPO_ROOT}/src/golf)
target_link_libraries(GolfCareerStatsTest PRIVATE crosspoint_test_common GTest::gtest_main)

gtest_discover_tests(GolfCareerStatsTest)
