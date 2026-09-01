add_executable(golf_player_select_tests test_golf_player_select.cpp)
target_link_libraries(golf_player_select_tests PRIVATE GTest::gtest_main crosspoint_test_common)
target_include_directories(golf_player_select_tests PRIVATE
  ${REPO_ROOT}/src
  ${REPO_ROOT}/src/activities/golf
)
gtest_discover_tests(golf_player_select_tests)
