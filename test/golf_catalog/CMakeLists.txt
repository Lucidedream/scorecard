add_executable(golf_catalog_tests
  test_golf_catalog.cpp
  ${REPO_ROOT}/src/golf/GolfCatalog.cpp
  ${REPO_ROOT}/src/golf/GolfCourseCatalogInfo.cpp
  ${REPO_ROOT}/src/golf/GolfCourseParse.cpp
  ${REPO_ROOT}/src/golf/GolfCourseValidate.cpp
  ${REPO_ROOT}/src/golf/GolfCourseMaps.cpp
  ${REPO_ROOT}/src/golf/GolfRoundDecode.cpp
  ${REPO_ROOT}/src/golf/GolfValidate.cpp
  ${REPO_ROOT}/src/golf/CourseStore.cpp
  ${REPO_ROOT}/src/golf/CourseOrder.cpp
  ${REPO_ROOT}/src/golf/GolfPaths.cpp
  ${REPO_ROOT}/src/golf/GolfCsv.cpp
  ${REPO_ROOT}/src/golf/GolfStats.cpp
  ${REPO_ROOT}/src/golf/GolfPenalty.cpp
  ${REPO_ROOT}/src/golf/GolfRules.cpp
)
target_link_libraries(golf_catalog_tests PRIVATE GTest::gtest_main ArduinoJson crosspoint_test_common)
target_include_directories(golf_catalog_tests PRIVATE
  stubs ${REPO_ROOT}/test/minibidi_arabic/stubs ${REPO_ROOT}/src/golf ${REPO_ROOT}/lib/Memory)
target_compile_definitions(golf_catalog_tests PRIVATE CROSSPOINT_GOLF=1)
gtest_discover_tests(golf_catalog_tests)
