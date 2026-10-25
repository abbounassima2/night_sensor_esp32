#ifndef DHT_HPP
#define DHT_HPP

#include <stdint.h>
#include "driver/gpio.h"

class DHT
{
public:
DHT(gpio_num_t pin);

bool init();

bool read(float& temperature,
          float& humidity);
private:
gpio_num_t data_pin;


bool wait_for_level(int level,
                    uint32_t timeout_us);

bool read_bit(uint8_t& bit);

bool read_data(uint8_t data[5]);


};

#endif
