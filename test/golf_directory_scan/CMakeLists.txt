# GolfDirectoryScan.h is a header-only pure predicate shared by every golf
# directory scan (GolfTipsStore, CourseStore, RoundArchive); the scans
# themselves need HalStorage/real SD access and are not host-testable.
add_executable(golf_directory_scan_tests test_golf_directory_scan.cpp)
target_link_libraries(golf_directory_scan_tests PRIVATE GTest::gtest_main crosspoint_test_common)
target_include_directories(golf_directory_scan_tests PRIVATE ${REPO_ROOT}/src/golf)
gtest_discover_tests(golf_directory_scan_tests)
