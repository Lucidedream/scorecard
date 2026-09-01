add_executable(golf_trends_tests test_golf_trends.cpp
  ${REPO_ROOT}/src/golf/GolfCsv.cpp
  ${REPO_ROOT}/src/golf/GolfHistory.cpp
  ${REPO_ROOT}/src/golf/GolfTrends.cpp
  ${REPO_ROOT}/src/golf/GolfStats.cpp
  ${REPO_ROOT}/src/golf/GolfPenalty.cpp
  ${REPO_ROOT}/src/golf/GolfRules.cpp
  ${REPO_ROOT}/test/golf_test_support/GolfPathsHost.cpp
)
target_link_libraries(golf_trends_tests PRIVATE GTest::gtest_main crosspoint_test_common)
target_include_directories(golf_trends_tests PRIVATE ${REPO_ROOT}/src/golf)
target_compile_definitions(golf_trends_tests PRIVATE CROSSPOINT_GOLF=1)
gtest_discover_tests(golf_trends_tests)
