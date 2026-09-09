add_executable(landscapecutter_unit_tests
    snip/SnapshotBatchTests.cpp
    snip/SelectionModelTests.cpp
    snip/SnapshotImageTests.cpp
    app/AppMetadataTests.cpp
    app/LaunchOptionsTests.cpp
    app/CaptureCoordinatorTests.cpp
    app/CaptureRuntimePolicyTests.cpp
    platform/MonitorTypesTests.cpp
    platform/windows/DisplayCatalogTests.cpp
)

include(Catch)

add_executable(landscapecutter_annotation_tests
    annotation/AnnotationDocumentTests.cpp
    annotation/AnnotationHistoryTests.cpp
)

target_link_libraries(landscapecutter_annotation_tests PRIVATE
    Catch2::Catch2WithMain
    landscapecutter_app_core
)

lc_enable_warnings(landscapecutter_annotation_tests)

catch_discover_tests(landscapecutter_annotation_tests)

target_link_libraries(landscapecutter_unit_tests PRIVATE
    Catch2::Catch2WithMain
    landscapecutter_app_core
)

lc_enable_warnings(landscapecutter_unit_tests)
catch_discover_tests(landscapecutter_unit_tests)
add_custom_command(TARGET landscapecutter_unit_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:libjpeg-turbo::jpeg>" "$<TARGET_FILE_DIR:landscapecutter_unit_tests>"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:landscapecutter_unit_tests>/imageformats"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Qt6::QJpegPlugin>" "$<TARGET_FILE_DIR:landscapecutter_unit_tests>/imageformats"
)

add_executable(landscapecutter_app_controller_tests
    snip/SnipOverlayTests.cpp
    snip/SnipSessionTests.cpp
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

add_executable(landscapecutter_dpi_tests
    platform/windows/DpiAwarenessTests.cpp
    ${CMAKE_SOURCE_DIR}/resources/windows/LandscapeCutter.rc
)

target_link_libraries(landscapecutter_dpi_tests PRIVATE
    Catch2::Catch2WithMain
    landscapecutter_app_core
)

target_link_options(landscapecutter_dpi_tests PRIVATE
    /MANIFEST:NO
)

lc_enable_warnings(landscapecutter_dpi_tests)

catch_discover_tests(landscapecutter_dpi_tests)

add_executable(landscapecutter_platform_message_tests
    platform/windows/NativeMessageWindowTests.cpp
    platform/windows/GlobalHotkeyServiceTests.cpp
)

target_link_libraries(landscapecutter_platform_message_tests PRIVATE
    Catch2::Catch2
    landscapecutter_app_core
)

lc_enable_warnings(landscapecutter_platform_message_tests)

catch_discover_tests(landscapecutter_platform_message_tests)

add_executable(landscapecutter_single_instance_tests
    platform/windows/SingleInstanceCoordinatorTests.cpp
)

target_link_libraries(landscapecutter_single_instance_tests PRIVATE
    Catch2::Catch2
    landscapecutter_app_core
    ole32
)

lc_enable_warnings(landscapecutter_single_instance_tests)

catch_discover_tests(landscapecutter_single_instance_tests)

add_executable(landscapecutter_graphics_tests
    graphics/d3d11/D3d11DeviceManagerTests.cpp
    graphics/d3d11/TextureCopyTests.cpp
)

target_link_libraries(landscapecutter_graphics_tests PRIVATE
    Catch2::Catch2WithMain
    landscapecutter_app_core
)

lc_enable_warnings(landscapecutter_graphics_tests)

catch_discover_tests(landscapecutter_graphics_tests)

add_executable(landscapecutter_capture_tests
    capture/windows/OneShotCaptureStateTests.cpp
    capture/windows/GraphicsCaptureInteropTests.cpp
)
target_link_libraries(landscapecutter_capture_tests PRIVATE
    Catch2::Catch2
    landscapecutter_app_core
)
lc_enable_warnings(landscapecutter_capture_tests)
catch_discover_tests(landscapecutter_capture_tests)

add_executable(landscapecutter_desktop_capture_tests
    integration/windows/DesktopCaptureIntegrationTests.cpp
    ${CMAKE_SOURCE_DIR}/resources/windows/LandscapeCutter.rc
)
target_link_libraries(landscapecutter_desktop_capture_tests PRIVATE
    Catch2::Catch2 landscapecutter_app_core runtimeobject
)
target_link_options(landscapecutter_desktop_capture_tests PRIVATE /MANIFEST:NO)
lc_enable_warnings(landscapecutter_desktop_capture_tests)
add_custom_command(TARGET landscapecutter_desktop_capture_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:landscapecutter_desktop_capture_tests>/platforms"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
        "$<TARGET_FILE_DIR:landscapecutter_desktop_capture_tests>/platforms"
)
catch_discover_tests(landscapecutter_desktop_capture_tests
    PROPERTIES LABELS desktop-integration TIMEOUT 30
)

add_executable(landscapecutter_hotkey_occupier helpers/HotkeyOccupier.cpp)
target_link_libraries(landscapecutter_hotkey_occupier PRIVATE WIL::WIL user32)
target_compile_definitions(landscapecutter_hotkey_occupier PRIVATE UNICODE _UNICODE NOMINMAX)
lc_enable_warnings(landscapecutter_hotkey_occupier)

add_test(NAME app_process_smoke COMMAND LandscapeCutter --smoke-test)
set_tests_properties(app_process_smoke PROPERTIES TIMEOUT 10)

find_program(POWERSHELL_EXECUTABLE powershell REQUIRED)

add_test(NAME app_process_behavior COMMAND ${POWERSHELL_EXECUTABLE}
    -NoProfile -ExecutionPolicy Bypass
    -File ${CMAKE_CURRENT_SOURCE_DIR}/scripts/AppProcessBehaviorTests.ps1
    -Application $<TARGET_FILE:LandscapeCutter>
    -HotkeyOccupier $<TARGET_FILE:landscapecutter_hotkey_occupier>
)
set_tests_properties(app_process_behavior PROPERTIES TIMEOUT 60 SKIP_RETURN_CODE 125 RUN_SERIAL TRUE)

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

add_test(NAME cmake_preset_portability_behavior
    COMMAND ${POWERSHELL_EXECUTABLE}
        -NoProfile
        -ExecutionPolicy Bypass
        -File ${CMAKE_CURRENT_SOURCE_DIR}/scripts/CMakePresetPolicyTests.ps1
        -CMakeCommand ${CMAKE_COMMAND}
        -SourceRoot ${CMAKE_SOURCE_DIR}
)
set_tests_properties(cmake_preset_portability_behavior PROPERTIES TIMEOUT 30)
