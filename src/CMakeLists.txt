add_library(landscapecutter_app_core STATIC
    annotation/AnnotationDocument.cpp
    annotation/AnnotationDocument.hpp
    annotation/AnnotationHistory.cpp
    annotation/AnnotationHistory.hpp
    annotation/AnnotationTypes.hpp
    snip/SnipOverlay.cpp
    snip/SnipOverlay.hpp
    snip/SnipSession.cpp
    snip/SnipSession.hpp
    snip/SnapshotReadback.cpp
    snip/SelectionModel.cpp
    snip/SnapshotBatch.cpp
    snip/SnapshotBatch.hpp
    snip/SnapshotImage.cpp
    app/AppController.cpp
    app/CaptureRuntimePolicy.hpp
    app/CaptureRuntimePolicy.cpp
    app/CaptureCoordinator.cpp
    app/CaptureCoordinator.hpp
    app/LaunchOptions.cpp
    capture/CaptureFrame.hpp
    capture/CaptureResult.hpp
    capture/MonitorCaptureService.hpp
    capture/windows/GraphicsCaptureInterop.cpp
    capture/windows/GraphicsCaptureInterop.hpp
    capture/windows/MonitorCaptureService.cpp
    capture/windows/MonitorCaptureService.hpp
    capture/windows/OneShotCaptureState.hpp
    graphics/d3d11/D3d11DeviceFactory.cpp
    graphics/d3d11/D3d11DeviceFactory.hpp
    graphics/d3d11/D3d11DeviceManager.cpp
    graphics/d3d11/D3d11DeviceManager.hpp
    graphics/d3d11/TextureCopy.cpp
    graphics/d3d11/TextureCopy.hpp
    platform/MonitorTypes.cpp
    platform/windows/DisplayCatalog.cpp
    platform/windows/DisplayCatalog.hpp
    platform/windows/DisplayTopologySource.hpp
    platform/windows/DpiAwareness.cpp
    platform/windows/GlobalHotkeyService.cpp
    platform/windows/GlobalHotkeyService.hpp
    platform/windows/MonitorDescriptor.hpp
    platform/windows/NativeMessageWindow.cpp
    platform/windows/NativeMessageWindow.hpp
    platform/windows/SingleInstanceCoordinator.cpp
    platform/windows/SingleInstanceCoordinator.hpp
    platform/windows/WindowsDisplayTopologySource.cpp
    platform/windows/WindowsDisplayTopologySource.hpp
)

target_include_directories(landscapecutter_app_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(landscapecutter_app_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    spdlog::spdlog
    WIL::WIL
    d3d11
    dxgi
    dxguid
    shcore
    user32
    windowsapp
)

target_compile_definitions(landscapecutter_app_core PUBLIC
    NOMINMAX
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
)

lc_enable_warnings(landscapecutter_app_core)

qt_add_executable(LandscapeCutter WIN32
    main.cpp
    ../resources/resources.qrc
    ../resources/windows/LandscapeCutter.rc
)

target_link_libraries(LandscapeCutter PRIVATE
    landscapecutter_app_core
    runtimeobject
)

target_link_options(LandscapeCutter PRIVATE
    /MANIFEST:NO
)

lc_enable_warnings(LandscapeCutter)

add_custom_command(TARGET LandscapeCutter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:libjpeg-turbo::jpeg>" "$<TARGET_FILE_DIR:LandscapeCutter>"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:LandscapeCutter>/imageformats"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Qt6::QJpegPlugin>" "$<TARGET_FILE_DIR:LandscapeCutter>/imageformats"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:LandscapeCutter>/platforms"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
        "$<TARGET_FILE_DIR:LandscapeCutter>/platforms"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:LandscapeCutter>/imageformats"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:Qt6::QICOPlugin>"
        "$<TARGET_FILE_DIR:LandscapeCutter>/imageformats"
)
