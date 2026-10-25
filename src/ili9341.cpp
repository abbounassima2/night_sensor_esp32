#include "ili9341.hpp"

#include <stdint.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// for the esp_logi
static const char *TAG = "ili9341";

spi_device_handle_t ILI9341::lcd_device = nullptr;
spi_device_handle_t ILI9341::touch_device = nullptr;

void ILI9341::init(){
    // configure LCD pins
    gpio_config_t outputs{};
    outputs.pin_bit_mask =
        (1ULL << PIN_LCD_RESET) |
        (1ULL << PIN_LCD_DC);
    outputs.mode = GPIO_MODE_OUTPUT;
    outputs.pull_up_en = GPIO_PULLUP_DISABLE;
    outputs.pull_down_en = GPIO_PULLDOWN_DISABLE;
    outputs.intr_type = GPIO_INTR_DISABLE;

    ESP_ERROR_CHECK(gpio_config(&outputs));

    // Hardware reset. The RESET pin was configured as an output but never
    // driven, so its level was undefined and could hold the panel in reset.
    gpio_set_level(PIN_LCD_DC, 0);
    gpio_set_level(PIN_LCD_RESET, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_LCD_RESET, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(PIN_LCD_RESET, 1);
    vTaskDelay(pdMS_TO_TICKS(120));   // panel needs ~120 ms after reset

    // configure the SPI bus pins
    // creates a variable bus_config holds spi bus
    spi_bus_config_t bus_config{};
    bus_config.mosi_io_num = PIN_MOSI;
    bus_config.miso_io_num = PIN_MISO;
    bus_config.sclk_io_num = PIN_SCLK;
    bus_config.quadwp_io_num = -1;
    bus_config.quadhd_io_num = -1;
    bus_config.max_transfer_sz = MAX_TRANSFER_BYTES;
    // initialize the bus.
    // DMA must be ON: without DMA a transaction is limited to 64 bytes,
    // far too small for pixel data.
    ESP_ERROR_CHECK(spi_bus_initialize( SPI_HOST_USED , &bus_config , SPI_DMA_CH_AUTO));

    // configure the lcd
    //contains settings for the device
    spi_device_interface_config_t lcd_config{};
    lcd_config.clock_speed_hz = 20 * 1000 * 1000; // 20 MHz (was 10): faster redraws
    lcd_config.mode = 0;
    lcd_config.spics_io_num = PIN_LCD_CS;
    lcd_config.queue_size = 4;

    ESP_ERROR_CHECK(
        spi_bus_add_device(
            SPI_HOST_USED,
            &lcd_config,
            &lcd_device
        )
    );


    // Register the touchscreen on the same bus.
    spi_device_interface_config_t touch_config{};
    touch_config.clock_speed_hz = 2 * 1000 * 1000; // 2 MHz
    touch_config.mode = 0;
    touch_config.spics_io_num = PIN_TOUCH_CS;
    touch_config.queue_size = 4;

    // Register the device and obtain its handle
    ESP_ERROR_CHECK(
        spi_bus_add_device(
            SPI_HOST_USED,
            &touch_config,
            &touch_device
        )
    );

    ESP_LOGI(TAG, "SPI bus and devices initialized");
}



void ILI9341::send_command( uint8_t command){

    // D/C = 0 means command
    gpio_set_level(PIN_LCD_DC , 0);

    spi_transaction_t transaction = {};
    transaction.length = 8 ;
    transaction.tx_buffer = &command;
    ESP_ERROR_CHECK(
        spi_device_transmit(lcd_device, &transaction)
    );

}


void ILI9341::send_data(const uint8_t* data, size_t length){
    if (data == nullptr || length == 0) {
        return;
    }

    // D/C = 1 means data
    gpio_set_level(PIN_LCD_DC, 1);

    // One SPI transaction cannot exceed the bus' max_transfer_sz, and LVGL
    // hands us whole pixel blocks (many KB). Send them in chunks.
    while (length > 0) {
        size_t chunk = (length > MAX_TRANSFER_BYTES) ? MAX_TRANSFER_BYTES : length;

        spi_transaction_t transaction{};
        transaction.length = chunk * 8;      // length is in BITS
        transaction.tx_buffer = data;

        ESP_ERROR_CHECK(
            spi_device_transmit(lcd_device, &transaction)
        );

        data   += chunk;
        length -= chunk;
    }
}

void ILI9341::set_window(uint16_t x1, uint16_t y1,uint16_t x2, uint16_t y2)
    {
        uint8_t data[4];

        // set column range
        send_command(ILI9341_CASET);
        // >> 8 extracts the high byte and xFF extracts low byte
        data[0] = x1 >> 8;
        data[1] = x1 & 0xFF;
        data[2] = x2 >> 8 ;
        data[3] = x2 & 0xFF;

        send_data(data , 4);
        // SET ROW RANGE
        send_command(ILI9341_PASET);
        data[0] = y1 >> 8;
        data[1] = y1 & 0xFF;
        data[2] = y2 >> 8;
        data[3] = y2 & 0xFF;
        send_data(data, 4);

        // begin writing pixel data
        send_command(ILI9341_RAMWR);
    }
