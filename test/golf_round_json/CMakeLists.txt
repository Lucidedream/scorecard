add_executable(golf_round_json_tests
  test_golf_round_json.cpp
  ${REPO_ROOT}/src/golf/GolfStats.cpp
  ${REPO_ROOT}/src/golf/GolfRoundDecode.cpp
  ${REPO_ROOT}/src/golf/GolfValidate.cpp
  ${REPO_ROOT}/src/golf/GolfPenalty.cpp
  ${REPO_ROOT}/src/golf/GolfRules.cpp
  ${REPO_ROOT}/src/golf/GolfPaths.cpp
)
target_link_libraries(golf_round_json_tests PRIVATE GTest::gtest_main crosspoint_test_common ArduinoJson)
target_include_directories(golf_round_json_tests PRIVATE ${REPO_ROOT}/src/golf)
target_compile_definitions(golf_round_json_tests PRIVATE CROSSPOINT_GOLF=1)
gtest_discover_tests(golf_round_json_tests)
