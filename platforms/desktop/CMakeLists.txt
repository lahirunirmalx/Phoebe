cmake_minimum_required (VERSION 3.8)
project(DesktopBuild)

cmake_policy(SET CMP0077 NEW)
set(CMAKE_CXX_STANDARD 17)

add_definitions(-DPLATFORM_BUILD_DESKTOP)

# Lvgl
set(LV_CONF_INCLUDE_SIMPLE OFF)
set(LV_CONF_PATH ../../lv_conf.h)
add_subdirectory(dependencies/lvgl)

# Arduinojson
add_subdirectory(dependencies/ArduinoJson)

# SDL
find_package(SDL2 REQUIRED SDL2)
include_directories(PUBLIC ${SDL2_INCLUDE_DIRS})
target_include_directories(lvgl PUBLIC ${SDL2_INCLUDE_DIRS})

# libcurl (for AppClaudeMeter HTTP /usage polling)
find_package(CURL REQUIRED)

# Mooncake 
set(MOONCAKE_BUILD_EXAMPLE OFF)
add_subdirectory(dependencies/mooncake)

# Mooncake log
set(MOONCAKE_LOG_BUILD_EXAMPLE OFF)
add_subdirectory(dependencies/mooncake_log)

# Smooth UI toolkit
add_subdirectory(dependencies/smooth_ui_toolkit)

# Lgfx slim
set(LGFX_SLIM_BUILD_EXAMPLE OFF)
add_subdirectory(dependencies/lgfx_slim)

# App layer
file(GLOB_RECURSE APP_LAYER_SRCS
    app/*.c
    app/*.cc
    app/*.cpp
)
set(APP_LAYER_INCS
    app/
)

# Desktop source files
file(GLOB_RECURSE APP_DESKTOP_BUILD_SRCS
    platforms/desktop/*.cpp
    platforms/desktop/*.cc
    platforms/desktop/*.c
)
add_executable(app_desktop_build ${APP_DESKTOP_BUILD_SRCS} ${APP_LAYER_SRCS})
target_include_directories(app_desktop_build PUBLIC ${APP_LAYER_INCS})
target_compile_definitions(app_desktop_build PRIVATE PHOEBE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(app_desktop_build PUBLIC
    mooncake
    mooncake_log
    lvgl
    lvgl_examples
    lvgl_demos
    ${SDL2_LIBRARIES}
    ArduinoJson
    smooth_ui_toolkit
    lgfx_slim
    pthread
    CURL::libcurl
)

# Set build path
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build/desktop)

# Ensure build path exists
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR})

# Set output path for generated executable
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# # Set output path for generated library files
# set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})
