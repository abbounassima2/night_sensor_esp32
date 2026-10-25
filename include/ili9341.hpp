
#ifndef ILI9341_HPP
#define ILI9341_HPP

#include <stdint.h>
#include <stddef.h>   // size_t (was missing)
#include "driver/gpio.h"
#include "driver/spi_master.h"

#define ILI9341_SWRESET      0x01
#define ILI9341_SLPOUT       0x11
#define ILI9341_DISPON       0x29
#define ILI9341_CASET        0x2A  // Column address set
#define ILI9341_PASET        0x2B  // Page (row) address set
#define ILI9341_RAMWR        0x2C  // Write pixel data
#define ILI9341_MADCTL       0x36  // Memory access control (orientation, RGB/BGR order)
#define ILI9341_PIXFMT       0x3A  // Pixel format

class ILI9341
{
public:
    void init();
    void send_command(uint8_t command);
    void send_data(const uint8_t* data, size_t length);
    void set_window( uint16_t x1, uint16_t y1,uint16_t x2, uint16_t y2);

private:
    static constexpr gpio_num_t PIN_LCD_CS = GPIO_NUM_11;
    static constexpr gpio_num_t PIN_LCD_RESET = GPIO_NUM_12;
    static constexpr gpio_num_t PIN_LCD_DC = GPIO_NUM_13;

    static constexpr gpio_num_t PIN_MOSI = GPIO_NUM_14;
    static constexpr gpio_num_t PIN_SCLK = GPIO_NUM_15;
    static constexpr gpio_num_t PIN_MISO = GPIO_NUM_16;
    static constexpr gpio_num_t PIN_TOUCH_CS = GPIO_NUM_17;

    static constexpr spi_host_device_t SPI_HOST_USED = SPI2_HOST;

    
    static constexpr size_t MAX_TRANSFER_BYTES = 4096;

    static spi_device_handle_t lcd_device;
    static spi_device_handle_t touch_device;
};

#endif
