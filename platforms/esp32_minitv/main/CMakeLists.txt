file(GLOB_RECURSE APP_LAYER_SRCS
    ../../../app/*.c
    ../../../app/*.cc
    ../../../app/*.cpp
)

set(APP_LAYER_INCS
    ../../../app
)

file(GLOB_RECURSE MY_HAL_SRCS
    ./hal/*.c
    ./hal/*.cc
    ./hal/*.cpp
)

idf_component_register(SRCS "app_main.cpp" ${APP_LAYER_SRCS} ${MY_HAL_SRCS}
                    INCLUDE_DIRS "." ${APP_LAYER_INCS})

# The Freenove Mini TV panel is 240x240 (square), not the app's default
# 144x168 portrait. Override the app-layer SCREEN_W/H so AppClaudeMeter and the
# watch faces fill this panel. Desktop keeps the default.
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    PHOEBE_SCREEN_W=240
    PHOEBE_SCREEN_H=240)
