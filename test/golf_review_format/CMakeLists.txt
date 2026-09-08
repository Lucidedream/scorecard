find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/I18n.h
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/generate_labels.py
          ${REPO_ROOT}/lib/I18n/translations/english.yaml ${CMAKE_CURRENT_BINARY_DIR}/I18n.h
  DEPENDS generate_labels.py ${REPO_ROOT}/lib/I18n/translations/english.yaml
)

add_executable(golf_review_format_tests
  ${CMAKE_CURRENT_BINARY_DIR}/I18n.h
  test_golf_review_format.cpp
  ${REPO_ROOT}/src/activities/golf/GolfReviewFormat.cpp
  ${REPO_ROOT}/src/golf/GolfStats.cpp
  ${REPO_ROOT}/src/golf/GolfPenalty.cpp
  ${REPO_ROOT}/src/golf/GolfRules.cpp
)

target_compile_definitions(golf_review_format_tests PRIVATE CROSSPOINT_GOLF=1)
target_include_directories(golf_review_format_tests PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${REPO_ROOT}/src
  ${REPO_ROOT}/src/activities/golf
  ${REPO_ROOT}/src/golf
)
target_link_libraries(golf_review_format_tests PRIVATE crosspoint_test_common GTest::gtest_main)

gtest_discover_tests(golf_review_format_tests)
