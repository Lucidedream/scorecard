add_executable(golf_paths_tests
  test_golf_paths.cpp
  ${REPO_ROOT}/src/golf/GolfPaths.cpp
  ${REPO_ROOT}/src/golf/GolfArchiveMarker.cpp
)
target_link_libraries(golf_paths_tests PRIVATE GTest::gtest_main crosspoint_test_common)
target_include_directories(golf_paths_tests PRIVATE ${REPO_ROOT}/src/golf)
target_compile_definitions(golf_paths_tests PRIVATE CROSSPOINT_GOLF=1)
gtest_discover_tests(golf_paths_tests)
