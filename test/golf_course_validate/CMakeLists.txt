add_executable(golf_course_validate_tests
  test_golf_course_validate.cpp
  ${REPO_ROOT}/src/golf/GolfCourseValidate.cpp
)
target_link_libraries(golf_course_validate_tests PRIVATE GTest::gtest_main crosspoint_test_common)
target_include_directories(golf_course_validate_tests PRIVATE ${REPO_ROOT}/src/golf)
target_compile_definitions(golf_course_validate_tests PRIVATE CROSSPOINT_GOLF=1)
gtest_discover_tests(golf_course_validate_tests)
