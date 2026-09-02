add_library(landscapecutter_app_core STATIC
    app/LaunchOptions.cpp
)

target_include_directories(landscapecutter_app_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(landscapecutter_app_core PUBLIC
    Qt6::Core
)

lc_enable_warnings(landscapecutter_app_core)
