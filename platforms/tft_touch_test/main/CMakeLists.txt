idf_component_register(
    SRCS "app_main.cpp" "st7789.cpp" "touch_t9.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver        # spi_master, gpio, touch_pad all live in `driver` on IDF v4.4
)
