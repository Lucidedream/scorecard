add_executable(golf_validate_tests
  test_golf_validate.cpp
  ${REPO_ROOT}/src/golf/GolfValidate.cpp
  ${REPO_ROOT}/src/golf/GolfRules.cpp
)
target_link_libraries(golf_validate_tests PRIVATE GTest::gtest_main crosspoint_test_common)
target_include_directories(golf_validate_tests PRIVATE ${REPO_ROOT}/src/golf)
target_compile_definitions(golf_validate_tests PRIVATE CROSSPOINT_GOLF=1)
gtest_discover_tests(golf_validate_tests)
