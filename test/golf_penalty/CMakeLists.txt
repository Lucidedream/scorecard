add_executable(golf_penalty_tests
  test_golf_penalty.cpp
  ${REPO_ROOT}/src/golf/GolfPenalty.cpp
  ${REPO_ROOT}/src/golf/GolfRules.cpp
)
target_link_libraries(golf_penalty_tests PRIVATE GTest::gtest_main crosspoint_test_common)
target_include_directories(golf_penalty_tests PRIVATE ${REPO_ROOT}/src/golf)
target_compile_definitions(golf_penalty_tests PRIVATE CROSSPOINT_GOLF=1)
gtest_discover_tests(golf_penalty_tests)
