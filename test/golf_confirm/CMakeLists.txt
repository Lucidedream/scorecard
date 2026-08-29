add_executable(golf_confirm_tests
  test_golf_confirm.cpp
  ${REPO_ROOT}/src/golf/GolfConfirm.cpp
  ${REPO_ROOT}/src/golf/GolfRules.cpp
  ${REPO_ROOT}/src/golf/GolfStats.cpp
)
target_link_libraries(golf_confirm_tests PRIVATE GTest::gtest_main crosspoint_test_common)
target_include_directories(golf_confirm_tests PRIVATE ${REPO_ROOT}/src/golf)
target_compile_definitions(golf_confirm_tests PRIVATE CROSSPOINT_GOLF=1)
gtest_discover_tests(golf_confirm_tests)
