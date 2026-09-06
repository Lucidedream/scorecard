include(FetchContent)

# Matches the ArduinoJson version pinned in platformio.ini. The firmware round /
# state.json codec (GolfJson.h) is templated on ArduinoJson, so the host
# round-trip test needs the real library rather than a stub.
FetchContent_Declare(
  ArduinoJson
  GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
  GIT_TAG v7.4.2
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(ArduinoJson)

add_executable(golf_round_json_tests
  test_golf_round_json.cpp
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
