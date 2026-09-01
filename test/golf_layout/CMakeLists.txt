add_executable(golf_layout_tests
  test_golf_layout.cpp
)

target_include_directories(golf_layout_tests PRIVATE
  ${REPO_ROOT}/freeink-sdk/libs/ui/FreeInkUI/include
  ${REPO_ROOT}/src
  ${REPO_ROOT}/src/activities/golf
)
target_link_libraries(golf_layout_tests PRIVATE crosspoint_test_common GTest::gtest_main)

gtest_discover_tests(golf_layout_tests)
