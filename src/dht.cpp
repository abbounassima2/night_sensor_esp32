#include "dht.hpp"

#include <stdio.h>

#include "esp_rom_sys.h"

DHT::DHT(gpio_num_t pin)
{
data_pin = pin;
}

bool DHT::init()
{
gpio_config_t config = {};

config.pin_bit_mask = (1ULL << data_pin);
config.mode = GPIO_MODE_INPUT_OUTPUT_OD;
config.pull_up_en = GPIO_PULLUP_ENABLE;
config.pull_down_en = GPIO_PULLDOWN_DISABLE;
config.intr_type = GPIO_INTR_DISABLE;

esp_err_t result = gpio_config(&config);

if (result != ESP_OK)
{
    printf("DHT GPIO configuration failed\n");
    return false;
}

gpio_set_level(data_pin, 1);

return true;


}

bool DHT::wait_for_level(int level,
uint32_t timeout_us)
{
uint32_t elapsed = 0;

while (gpio_get_level(data_pin) != level)
{
    esp_rom_delay_us(1);

    elapsed++;

    if (elapsed >= timeout_us)
    {
        return false;
    }
}

return true;

}

bool DHT::read_bit(uint8_t& bit)
{
/*
* Each DHT22 bit begins with a LOW pulse,
* followed by a HIGH pulse.
*/

if (!wait_for_level(0, 100))
{
    return false;
}

if (!wait_for_level(1, 100))
{
    return false;
}

/*
 * Sample the HIGH pulse.
 *
 * A shorter HIGH pulse represents 0.
 * A longer HIGH pulse represents 1.
 */

esp_rom_delay_us(40);

if (gpio_get_level(data_pin) == 1)
{
    bit = 1;
}
else
{
    bit = 0;
}

/*
 * Wait until the HIGH pulse has finished.
 */

if (!wait_for_level(0, 100))
{
    return false;
}

return true;


}

bool DHT::read_data(uint8_t data[5])
{
/*
* Send the DHT22 start signal.
*/


gpio_set_level(data_pin, 0);

/*
 * Keep DATA LOW for at least 1 ms.
 */

esp_rom_delay_us(2000);

/*
 * Release DATA.
 * Because this is open-drain, HIGH releases the line.
 */

gpio_set_level(data_pin, 1);

/*
 * DHT22 response:
 *
 * LOW
 * HIGH
 * LOW
 */

if (!wait_for_level(0, 100))
{
    printf("DHT response LOW timeout\n");
    return false;
}

if (!wait_for_level(1, 100))
{
    printf("DHT response HIGH timeout\n");
    return false;
}

if (!wait_for_level(0, 100))
{
    printf("DHT response data timeout\n");
    return false;
}

/*
 * Read 40 bits = 5 bytes.
 */

for (int byte = 0; byte < 5; byte++)
{
    data[byte] = 0;

    for (int bit_index = 0;
         bit_index < 8;
         bit_index++)
    {
        uint8_t bit = 0;

        if (!read_bit(bit))
        {
            return false;
        }

        data[byte] <<= 1;
        data[byte] |= bit;
    }
}

return true;


}

bool DHT::read(float& temperature,
float& humidity)
{
uint8_t data[5] = {};


if (!read_data(data))
{
    return false;
}

/*
 * DHT22 data format:
 *
 * data[0] = humidity high byte
 * data[1] = humidity low byte
 * data[2] = temperature high byte
 * data[3] = temperature low byte
 * data[4] = checksum
 */

uint8_t checksum =
    data[0] +
    data[1] +
    data[2] +
    data[3];

if (checksum != data[4])
{
    printf("DHT checksum error\n");
    return false;
}

/*
 * Reconstruct humidity.
 */

uint16_t humidity_raw =
    ((uint16_t)data[0] << 8) |
    data[1];

/*
 * Reconstruct temperature.
 */

uint16_t temperature_raw =
    ((uint16_t)data[2] << 8) |
    data[3];

/*
 * Convert humidity.
 */

humidity =
    humidity_raw / 10.0f;

/*
 * Convert temperature.
 *
 * Bit 15 is the sign bit.
 */

if (temperature_raw & 0x8000)
{
    temperature_raw &= 0x7FFF;

    temperature =
        -(temperature_raw / 10.0f);
}
else
{
    temperature =
        temperature_raw / 10.0f;
}

return true;

}


