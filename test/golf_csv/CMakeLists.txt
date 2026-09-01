add_executable(golf_csv_tests test_golf_csv.cpp
  ${REPO_ROOT}/src/golf/GolfCsv.cpp
  ${REPO_ROOT}/src/golf/GolfStats.cpp
  ${REPO_ROOT}/src/golf/GolfPenalty.cpp
  ${REPO_ROOT}/src/golf/GolfRules.cpp
  ${REPO_ROOT}/test/golf_test_support/GolfPathsHost.cpp
)
target_link_libraries(golf_csv_tests PRIVATE GTest::gtest_main crosspoint_test_common)
target_include_directories(golf_csv_tests PRIVATE ${REPO_ROOT}/src/golf)
target_compile_definitions(golf_csv_tests PRIVATE CROSSPOINT_GOLF=1)
gtest_discover_tests(golf_csv_tests)
