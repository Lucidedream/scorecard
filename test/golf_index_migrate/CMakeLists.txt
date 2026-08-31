add_executable(golf_index_migrate_tests test_golf_index_migrate.cpp
  ${REPO_ROOT}/src/golf/GolfCsv.cpp
  ${REPO_ROOT}/src/golf/GolfIndexMigrate.cpp
)
target_link_libraries(golf_index_migrate_tests PRIVATE GTest::gtest_main crosspoint_test_common)
target_include_directories(golf_index_migrate_tests PRIVATE ${REPO_ROOT}/src/golf)
target_compile_definitions(golf_index_migrate_tests PRIVATE CROSSPOINT_GOLF=1)
gtest_discover_tests(golf_index_migrate_tests)
