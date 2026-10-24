add_executable(golf_nine_picker_tests
  test_golf_nine_picker.cpp
  ${REPO_ROOT}/src/golf/CourseOrder.cpp
)
target_link_libraries(golf_nine_picker_tests PRIVATE GTest::gtest_main crosspoint_test_common)
target_include_directories(golf_nine_picker_tests PRIVATE
  ${REPO_ROOT}/src/golf
  ${REPO_ROOT}/test/minibidi_arabic/stubs
)
target_compile_definitions(golf_nine_picker_tests PRIVATE CROSSPOINT_GOLF=1)
gtest_discover_tests(golf_nine_picker_tests)
