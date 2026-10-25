#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"      
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "dht.hpp"
#include "lcd.hpp"


static const char *TAG = "NIGHT_BUILD_DEMO";


static constexpr gpio_num_t PIN_DHT = GPIO_NUM_4;     

static constexpr int64_t SENSOR_WARMUP_MS  = 1500;    // DHT22 needs ~1 s after power-up
static constexpr int64_t SENSOR_PERIOD_MS  = 2500;    // DHT22 max rate is one read / 2 s
static constexpr int     MAX_FAILED_READS  = 3;       
static constexpr uint32_t LVGL_PERIOD_MS   = 10;


static DHT dht(PIN_DHT);
static LCD lcd;


static int64_t now_ms()
{
    return esp_timer_get_time() / 1000;
}


static void set_clock_from_build_time()
{
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

    char month_name[4] = {};
    int day = 1, year = 1970, hour = 0, minute = 0, second = 0;

    sscanf(__DATE__, "%3s %d %d", month_name, &day, &year);   
    sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);    

    const char *found = strstr(months, month_name);
    int month = found ? (int)(found - months) / 3 : 0;

    struct tm build_time = {};
    build_time.tm_year  = year - 1900;
    build_time.tm_mon   = month;
    build_time.tm_mday  = day;
    build_time.tm_hour  = hour;
    build_time.tm_min   = minute;
    build_time.tm_sec   = second;
    build_time.tm_isdst = -1;

    struct timeval tv = {};
    tv.tv_sec  = mktime(&build_time);
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);

    ESP_LOGI(TAG, "clock set to build time: %s %s", __DATE__, __TIME__);
}


static void ui_task(void *)
{
    set_clock_from_build_time();

    if (!dht.init())
    {
        ESP_LOGE(TAG, "DHT init failed");
    }

    lcd.init();
    lcd.draw_ui();

    float temperature = 0.0f;
    float humidity = 0.0f;
    int failed_reads = 0;

    int64_t next_read_ms = now_ms() + SENSOR_WARMUP_MS;

    while (true)
    {
        if (now_ms() >= next_read_ms)
        {
            next_read_ms = now_ms() + SENSOR_PERIOD_MS;

            if (dht.read(temperature, humidity))
            {
                failed_reads = 0;
                lcd.set_readings(temperature, humidity, true);

                ESP_LOGI(TAG, "T = %.1f C, RH = %.1f %%", temperature, humidity);
            }
            else
            {
                failed_reads++;
                ESP_LOGW(TAG, "DHT read failed (%d in a row)", failed_reads);

                // One lost frame is normal; keep showing the last good value
                // and only complain when it keeps happening.
                if (failed_reads >= MAX_FAILED_READS)
                {
                    lcd.set_readings(0.0f, 0.0f, false);
                }
            }
        }

        lcd.display();                              
        vTaskDelay(pdMS_TO_TICKS(LVGL_PERIOD_MS));  
    }
}


extern "C" void app_main(void)
{
    BaseType_t created = xTaskCreate(ui_task, "ui_task", 8192, nullptr, 5, nullptr);

    if (created != pdPASS)
    {
        ESP_LOGE(TAG, "could not create ui_task");
    }
}

