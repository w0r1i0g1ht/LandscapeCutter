add_library(landscapecutter_app_core STATIC
    app/AppController.cpp
    app/LaunchOptions.cpp
    platform/MonitorTypes.cpp
    platform/windows/DpiAwareness.cpp
    platform/windows/GlobalHotkeyService.cpp
    platform/windows/GlobalHotkeyService.hpp
    platform/windows/NativeMessageWindow.cpp
    platform/windows/NativeMessageWindow.hpp
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
    user32
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
