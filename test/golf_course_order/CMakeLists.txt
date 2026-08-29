add_executable(golf_course_order_tests
  test_golf_course_order.cpp
  ${REPO_ROOT}/src/golf/CourseOrder.cpp
)
target_link_libraries(golf_course_order_tests PRIVATE GTest::gtest_main crosspoint_test_common)
target_include_directories(golf_course_order_tests PRIVATE ${REPO_ROOT}/src/golf)
target_compile_definitions(golf_course_order_tests PRIVATE CROSSPOINT_GOLF=1)
gtest_discover_tests(golf_course_order_tests)
