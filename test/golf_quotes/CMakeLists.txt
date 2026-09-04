add_executable(golf_quotes_tests test_golf_quotes.cpp
  ${REPO_ROOT}/src/golf/GolfQuotes.cpp
)
target_link_libraries(golf_quotes_tests PRIVATE GTest::gtest_main crosspoint_test_common)
target_include_directories(golf_quotes_tests PRIVATE ${REPO_ROOT}/src/golf)
target_compile_definitions(golf_quotes_tests PRIVATE CROSSPOINT_GOLF=1)
gtest_discover_tests(golf_quotes_tests)
