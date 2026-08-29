add_executable(GolfRulesTest
  GolfRulesTest.cpp
  ${REPO_ROOT}/src/golf/GolfRules.cpp
)

target_compile_definitions(GolfRulesTest PRIVATE CROSSPOINT_GOLF=1)
target_include_directories(GolfRulesTest PRIVATE ${REPO_ROOT}/src/golf)
target_link_libraries(GolfRulesTest PRIVATE crosspoint_test_common GTest::gtest_main)

gtest_discover_tests(GolfRulesTest)
