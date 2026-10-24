add_executable(golf_course_validate_tests
  test_golf_course_validate.cpp
  ${REPO_ROOT}/src/golf/GolfCourseParse.cpp
  ${REPO_ROOT}/src/golf/CourseOrder.cpp
  ${REPO_ROOT}/src/golf/GolfRoundDecode.cpp
  ${REPO_ROOT}/src/golf/GolfValidate.cpp
  ${REPO_ROOT}/src/golf/GolfPenalty.cpp
  ${REPO_ROOT}/src/golf/GolfRules.cpp
  ${REPO_ROOT}/src/golf/GolfCourseValidate.cpp
)
target_link_libraries(golf_course_validate_tests PRIVATE GTest::gtest_main crosspoint_test_common)
target_include_directories(golf_course_validate_tests PRIVATE ${REPO_ROOT}/src/golf)
target_compile_definitions(golf_course_validate_tests PRIVATE CROSSPOINT_GOLF=1)
gtest_discover_tests(golf_course_validate_tests)

target_link_libraries(golf_course_validate_tests PRIVATE ArduinoJson)
target_compile_definitions(golf_course_validate_tests PRIVATE GOLF_EXAMPLES_DIRECTORY="${REPO_ROOT}/docs/golf/examples")

target_include_directories(golf_course_validate_tests PRIVATE ${REPO_ROOT}/test/minibidi_arabic/stubs)
