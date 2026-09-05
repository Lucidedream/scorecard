find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/I18n.h
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/generate_labels.py
          ${REPO_ROOT}/lib/I18n/translations/english.yaml ${CMAKE_CURRENT_BINARY_DIR}/I18n.h
  DEPENDS generate_labels.py ${REPO_ROOT}/lib/I18n/translations/english.yaml
)
add_library(golf_export_test_support STATIC
  ${CMAKE_CURRENT_BINARY_DIR}/I18n.h
  ${REPO_ROOT}/src/golf/GolfExportStrings.cpp
  ${REPO_ROOT}/src/golf/GolfRoundExport.cpp
  ${REPO_ROOT}/src/golf/GolfQrCode.cpp
  ${REPO_ROOT}/src/golf/GolfStats.cpp
  ${REPO_ROOT}/src/golf/GolfPenalty.cpp
  ${REPO_ROOT}/src/golf/GolfRules.cpp
  ${REPO_ROOT}/src/golf/GolfPaths.cpp
)
if(UNIX)
  target_sources(golf_export_test_support PRIVATE ${REPO_ROOT}/src/golf/GolfExportServer.cpp)
endif()
target_compile_definitions(golf_export_test_support PUBLIC CROSSPOINT_GOLF=1)
target_include_directories(golf_export_test_support PUBLIC ${REPO_ROOT}/src/golf PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(golf_export_test_support PUBLIC crosspoint_test_common)
add_executable(golf_export_tests GolfExportTest.cpp)
target_link_libraries(golf_export_tests PRIVATE golf_export_test_support GTest::gtest_main)
gtest_discover_tests(golf_export_tests)
add_executable(golf_export_fixture GolfExportFixture.cpp)
target_link_libraries(golf_export_fixture PRIVATE golf_export_test_support)
add_test(NAME GolfExportFormats.IndependentParsers
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/verify_formats.py $<TARGET_FILE:golf_export_fixture>)
