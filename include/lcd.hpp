#ifndef LCD_HPP
#define LCD_HPP

#include "ili9341.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#include "lvgl.h"
#pragma GCC diagnostic pop

class LCD
{
public:
    void init();
    void draw_ui();
    void display();                                   
    void set_readings(float temperature,             
                      float humidity,
                      bool valid);

private:
    void update_clock();
    static void flush_cb(lv_disp_drv_t *drv,
                         const lv_area_t *area,
                         lv_color_t *color_p);

    ILI9341 display_driver;

    // LVGL necessities
    lv_disp_draw_buf_t draw_buf;
    lv_disp_drv_t disp_drv;
    lv_color_t *buffer = nullptr;

    lv_obj_t *temp_label = nullptr;
    lv_obj_t *humidity_label = nullptr;
    lv_obj_t *time_label = nullptr;
    lv_obj_t *date_label = nullptr;
    lv_obj_t *status_label = nullptr;

    int last_second = -1;
};

#endif