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

add_executable(landscapecutter_app_controller_tests
    app/AppControllerTests.cpp
    ../resources/resources.qrc
)

target_link_libraries(landscapecutter_app_controller_tests PRIVATE
    Catch2::Catch2WithMain
    landscapecutter_app_core
)

lc_enable_warnings(landscapecutter_app_controller_tests)

add_custom_command(TARGET landscapecutter_app_controller_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:landscapecutter_app_controller_tests>/platforms"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
        "$<TARGET_FILE_DIR:landscapecutter_app_controller_tests>/platforms"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:landscapecutter_app_controller_tests>/imageformats"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:Qt6::QICOPlugin>"
        "$<TARGET_FILE_DIR:landscapecutter_app_controller_tests>/imageformats"
)

catch_discover_tests(landscapecutter_app_controller_tests
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_test(NAME app_process_smoke COMMAND LandscapeCutter --smoke-test)
set_tests_properties(app_process_smoke PROPERTIES TIMEOUT 10)
