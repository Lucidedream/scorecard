add_executable(golf_round_file_tests
  test_golf_round_file.cpp
  ${REPO_ROOT}/src/golf/GolfRoundDecode.cpp
  ${REPO_ROOT}/src/golf/GolfValidate.cpp
)
target_link_libraries(golf_round_file_tests PRIVATE GTest::gtest_main crosspoint_test_common)
target_include_directories(golf_round_file_tests PRIVATE ${REPO_ROOT}/src/golf)
target_compile_definitions(golf_round_file_tests PRIVATE CROSSPOINT_GOLF=1)
gtest_discover_tests(golf_round_file_tests)
