# Main component of the project.
#
# FIX: the generated file only listed "main.cpp", so lcd.cpp, ili9341.cpp and
# dht.cpp were never compiled -> "undefined reference to LCD::init" etc.
# Every .cpp file must be listed here (headers are found through INCLUDE_DIRS).

idf_component_register(
    SRCS
        "main.cpp"
        "lcd.cpp"
        "ili9341.cpp"
        "dht.cpp"
    INCLUDE_DIRS
        "."
    REQUIRES
        esp_driver_spi      # driver/spi_master.h
        esp_driver_gpio     # driver/gpio.h
        esp_timer           # esp_timer.h (dht.cpp, main.cpp, LVGL tick)
        esp_rom             # esp_rom_sys.h (esp_rom_delay_us)
        freertos            # tasks, vTaskDelay, portENTER_CRITICAL
        heap                # heap_caps_malloc (DMA draw buffer)
        log                 # ESP_LOGx
)

# LVGL is not listed on purpose: with PlatformIO it comes from
#   lib_deps = lvgl/lvgl@8.3.9   in platformio.ini.
# If you use the IDF component manager instead, add "lvgl__lvgl" to REQUIRES
# and declare the dependency in idf_component.yml.