add_executable(landscapecutter_unit_tests
    app/AppMetadataTests.cpp
    app/LaunchOptionsTests.cpp
)

target_link_libraries(landscapecutter_unit_tests PRIVATE
    Catch2::Catch2WithMain
    landscapecutter_app_core
)

lc_enable_warnings(landscapecutter_unit_tests)

include(Catch)
catch_discover_tests(landscapecutter_unit_tests)

add_test(NAME app_process_smoke COMMAND LandscapeCutter --smoke-test)
set_tests_properties(app_process_smoke PROPERTIES TIMEOUT 10)
