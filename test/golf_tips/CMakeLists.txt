# The SD-facing GolfTipsStore lives in GolfTipsStore.cpp and pulls in
# firmware-only headers; the host suite builds the streaming parsers and the
# list-state policy only, which is where the format rules live.
add_executable(golf_tips_tests test_golf_tips.cpp
  ${REPO_ROOT}/src/golf/GolfTips.cpp
)
target_link_libraries(golf_tips_tests PRIVATE GTest::gtest_main crosspoint_test_common)
target_include_directories(golf_tips_tests PRIVATE ${REPO_ROOT}/src/golf)
target_compile_definitions(golf_tips_tests PRIVATE
  CROSSPOINT_GOLF=1
  GOLF_TIPS_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
gtest_discover_tests(golf_tips_tests)
