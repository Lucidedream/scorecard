add_executable(golf_catalog_check
  golf_catalog_check.cpp
  ${REPO_ROOT}/src/golf/GolfCourseParse.cpp
  ${REPO_ROOT}/src/golf/CourseOrder.cpp
  ${REPO_ROOT}/src/golf/GolfRoundDecode.cpp
  ${REPO_ROOT}/src/golf/GolfValidate.cpp
  ${REPO_ROOT}/src/golf/GolfPenalty.cpp
  ${REPO_ROOT}/src/golf/GolfRules.cpp
  ${REPO_ROOT}/src/golf/GolfCourseValidate.cpp
)
target_link_libraries(golf_catalog_check PRIVATE ArduinoJson crosspoint_test_common)
target_include_directories(golf_catalog_check PRIVATE
  ${REPO_ROOT}/src/golf
  ${REPO_ROOT}/test/minibidi_arabic/stubs
)
target_compile_definitions(golf_catalog_check PRIVATE CROSSPOINT_GOLF=1)
add_test(NAME golf_catalog_check COMMAND golf_catalog_check)
set_tests_properties(golf_catalog_check PROPERTIES SKIP_RETURN_CODE 77)
