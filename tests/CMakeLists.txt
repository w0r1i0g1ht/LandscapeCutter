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

find_program(POWERSHELL_EXECUTABLE powershell REQUIRED)

add_test(NAME bootstrap_identity_behavior
    COMMAND ${POWERSHELL_EXECUTABLE}
        -NoProfile
        -ExecutionPolicy Bypass
        -File ${CMAKE_CURRENT_SOURCE_DIR}/scripts/BootstrapTests.ps1
        -BootstrapScript ${CMAKE_SOURCE_DIR}/scripts/bootstrap.ps1
        -KnownGoodVcpkgRoot ${CMAKE_SOURCE_DIR}/.tools/vcpkg
)
set_tests_properties(bootstrap_identity_behavior PROPERTIES TIMEOUT 120)

add_test(NAME cmake_sdk_floor_behavior
    COMMAND ${POWERSHELL_EXECUTABLE}
        -NoProfile
        -ExecutionPolicy Bypass
        -File ${CMAKE_CURRENT_SOURCE_DIR}/scripts/CMakeSdkFloorTests.ps1
        -CMakeCommand ${CMAKE_COMMAND}
        -SourceRoot ${CMAKE_SOURCE_DIR}
)
set_tests_properties(cmake_sdk_floor_behavior PROPERTIES TIMEOUT 30)
