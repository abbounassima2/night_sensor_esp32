
#include "lcd.hpp"
#include "ili9341.hpp"

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"      

static const char *TAG = "lcd";

static constexpr int LCD_WIDTH     = 320;   
static constexpr int LCD_HEIGHT    = 240;
static constexpr int LCD_BUF_LINES = 30;    


static constexpr uint8_t MADCTL_LANDSCAPE = 0x28;



struct InitCommand
{
    uint8_t command;
    uint8_t data[15];
    uint8_t length;
};

static const InitCommand init_sequence[] =
{
    {0xEF, {0x03, 0x80, 0x02}, 3},
    {0xCF, {0x00, 0xC1, 0x30}, 3},                        // power control B
    {0xED, {0x64, 0x03, 0x12, 0x81}, 4},                  // power on sequence
    {0xE8, {0x85, 0x00, 0x78}, 3},                        // driver timing A
    {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},            // power control A
    {0xF7, {0x20}, 1},                                    // pump ratio
    {0xEA, {0x00, 0x00}, 2},                              // driver timing B
    {0xC0, {0x23}, 1},                                    // power control 1
    {0xC1, {0x10}, 1},                                    // power control 2
    {0xC5, {0x3E, 0x28}, 2},                              // VCOM 1
    {0xC7, {0x86}, 1},                                    // VCOM 2
    {ILI9341_MADCTL, {MADCTL_LANDSCAPE}, 1},              // orientation
    {ILI9341_PIXFMT, {0x55}, 1},                          // 16 bit / pixel (RGB565)
    {0xB1, {0x00, 0x18}, 2},                              // frame rate
    {0xB6, {0x08, 0x82, 0x27}, 3},                        // display function
    {0xF2, {0x00}, 1},                                    // 3-gamma off
    {0x26, {0x01}, 1},                                    // gamma curve 1
    {0xE0, {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
            0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00}, 15},   // positive gamma
    {0xE1, {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
            0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F}, 15},   // negative gamma
};


void LCD::flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    LCD *self = static_cast<LCD *>(drv->user_data);

    uint32_t width  = area->x2 - area->x1 + 1;
    uint32_t height = area->y2 - area->y1 + 1;

    self->display_driver.set_window(area->x1, area->y1, area->x2, area->y2);
    self->display_driver.send_data((const uint8_t *)color_p,
                                   width * height * sizeof(lv_color_t));

   
    lv_disp_flush_ready(drv);
}

void LCD::init(){
   
    display_driver.init();

    // 2. Pixel buffer. Must be DMA-capable internal RAM for the SPI driver.
    const size_t buffer_pixels = LCD_WIDTH * LCD_BUF_LINES;
    buffer = static_cast<lv_color_t *>(
        heap_caps_malloc(buffer_pixels * sizeof(lv_color_t), MALLOC_CAP_DMA));
    ESP_ERROR_CHECK(buffer ? ESP_OK : ESP_ERR_NO_MEM);

    // 3. Panel start-up
    display_driver.send_command(ILI9341_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));

    for (const InitCommand &step : init_sequence)
    {
        display_driver.send_command(step.command);
        display_driver.send_data(step.data, step.length);
    }

    display_driver.send_command(ILI9341_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));

    // Paint the GRAM black BEFORE switching the display on,
    // otherwise random power-up noise flashes on screen.
    memset(buffer, 0, buffer_pixels * sizeof(lv_color_t));
    display_driver.set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    for (int line = 0; line < LCD_HEIGHT; line += LCD_BUF_LINES)
    {
        int lines = (LCD_HEIGHT - line < LCD_BUF_LINES) ? (LCD_HEIGHT - line) : LCD_BUF_LINES;
        display_driver.send_data((const uint8_t *)buffer,
                                 LCD_WIDTH * lines * sizeof(lv_color_t));
    }

    display_driver.send_command(ILI9341_DISPON);
    vTaskDelay(pdMS_TO_TICKS(20));

    // 4. LVGL itself
    lv_init();

    lv_disp_draw_buf_init(&draw_buf, buffer, nullptr, buffer_pixels);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res   = LCD_WIDTH;
    disp_drv.ver_res   = LCD_HEIGHT;
    disp_drv.flush_cb  = LCD::flush_cb;
    disp_drv.draw_buf  = &draw_buf;
    disp_drv.user_data = this;                 // lets flush_cb find this object
    lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "panel + LVGL ready");
}

/* ------------------------------------------------------------------ */
/*  UI                                                                 */
/* ------------------------------------------------------------------ */

static lv_style_t circle_style;
static bool circle_style_ready = false;

static lv_obj_t *make_circle(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                             lv_coord_t diameter, uint32_t color)
{
    lv_obj_t *circle = lv_obj_create(parent);

    lv_obj_remove_style_all(circle);                       // drop the theme's white box look
    lv_obj_add_style(circle, &circle_style, LV_PART_MAIN);
    lv_obj_clear_flag(circle, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));

    lv_obj_set_size(circle, diameter, diameter);
    lv_obj_set_pos(circle, x, y);
    lv_obj_set_style_bg_color(circle, lv_color_hex(color), LV_PART_MAIN);

    return circle;
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font,
                            uint32_t color, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
    lv_label_set_text(label, text);

    return label;
}

// Animation callback: fades one star's opacity.
static void twinkle_cb(void *obj, int32_t value)
{
    lv_obj_set_style_bg_opa(static_cast<lv_obj_t *>(obj), (lv_opa_t)value, LV_PART_MAIN);
}

static void start_twinkle(lv_obj_t *star, uint32_t index)
{
    uint32_t duration = 900 + 350 * (index % 5);      // each star its own rhythm

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, star);
    lv_anim_set_exec_cb(&anim, twinkle_cb);
    lv_anim_set_values(&anim, LV_OPA_20, LV_OPA_COVER);
    lv_anim_set_time(&anim, duration);
    lv_anim_set_playback_time(&anim, duration);
    lv_anim_set_delay(&anim, 150 * index);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&anim);
}

struct Star
{
    lv_coord_t x;
    lv_coord_t y;
    lv_coord_t size;
    uint32_t color;
};


static const Star stars[] =
{
    { 40,  35, 6, 0x243F8A},   
    { 14,  50, 2, 0xFFFFFF},
    {100,  30, 3, 0xCFD8FF},
    {128,  48, 2, 0xFFFFFF},
    {160,  14, 3, 0xFFF2C0},
    {190,  62, 2, 0x9FB4FF},
    {110,  86, 2, 0xFFFFFF},
    { 62,  68, 3, 0x9FB4FF},
    { 22,  96, 2, 0xCFD8FF},
    {150,  92, 3, 0xFFFFFF},
    {300,  20, 3, 0xFFFFFF},
    {290,  66, 2, 0xCFD8FF},
    {196,  28, 2, 0xFFF2C0},
    {255,  98, 3, 0x9FB4FF},
    {230,  12, 2, 0xFFFFFF},
    {308,  96, 2, 0xFFFFFF},
    {180, 100, 2, 0xCFD8FF},
};

void LCD::draw_ui()
{
    lv_obj_t *screen = lv_scr_act();

    if (!circle_style_ready)
    {
        lv_style_init(&circle_style);
        lv_style_set_radius(&circle_style, LV_RADIUS_CIRCLE);
        lv_style_set_bg_opa(&circle_style, LV_OPA_COVER);
        lv_style_set_border_width(&circle_style, 0);
        lv_style_set_pad_all(&circle_style, 0);
        circle_style_ready = true;
    }

    // Black background
    lv_obj_set_style_bg_color(
        screen,
        lv_color_hex(0x000000),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        screen,
        LV_OPA_COVER,
        LV_PART_MAIN
    );
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    
    uint32_t index = 0;
    for (const Star &s : stars)
    {
        lv_obj_t *star = make_circle(screen, s.x, s.y, s.size, s.color);

        if (index % 3 == 1)            // every third star twinkles
        {
            start_twinkle(star, index);
        }
        index++;
    }

    
    make_circle(screen, 220, 40, 40, 0xFFD447);

    
    make_circle(screen, 232, 32, 35, 0x000000);

    // --- Clock -------------------------------------------------------
    status_label = make_label(screen, &lv_font_montserrat_14, 0x8A9BD0, "Reading sensor...");
    lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 8, 6);

    time_label = make_label(screen, &lv_font_montserrat_40, 0xE8EEFF, "--:--:--");
    lv_obj_align(time_label, LV_ALIGN_TOP_MID, 0, 108);

    date_label = make_label(screen, &lv_font_montserrat_14, 0x8A9BD0, "");
    lv_obj_align(date_label, LV_ALIGN_TOP_MID, 0, 156);

    // --- Sensor readings -------
    lv_obj_t *temp_caption = make_label(screen, &lv_font_montserrat_14, 0x8A9BD0, "Temperature");
    lv_obj_align(temp_caption, LV_ALIGN_TOP_MID, -80, 182);

    temp_label = make_label(screen, &lv_font_montserrat_28, 0xFF9A5A, "--.-");
    lv_obj_align(temp_label, LV_ALIGN_TOP_MID, -80, 200);

    lv_obj_t *humidity_caption = make_label(screen, &lv_font_montserrat_14, 0x8A9BD0, "Humidity");
    lv_obj_align(humidity_caption, LV_ALIGN_TOP_MID, 80, 182);

    humidity_label = make_label(screen, &lv_font_montserrat_28, 0x5CC8FF, "--.-");
    lv_obj_align(humidity_label, LV_ALIGN_TOP_MID, 80, 200);

    update_clock();
}

void LCD::set_readings(float temperature, float humidity, bool valid)
{
    if (!temp_label || !humidity_label || !status_label)
    {
        return;     
    }

    char text[24];

    if (valid)
    {
       
        snprintf(text, sizeof(text), "%.1f\xC2\xB0" "C", temperature);
        lv_label_set_text(temp_label, text);

        snprintf(text, sizeof(text), "%.1f%%", humidity);
        lv_label_set_text(humidity_label, text);

        lv_label_set_text(status_label, "Sensor OK");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x5FD38D), LV_PART_MAIN);
    }
    else
    {
        lv_label_set_text(temp_label, "--.-");
        lv_label_set_text(humidity_label, "--.-");

        lv_label_set_text(status_label, "Sensor error");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xFF6B6B), LV_PART_MAIN);
    }
}

void LCD::update_clock()
{
    if (!time_label || !date_label)
    {
        return;
    }

    time_t now = time(nullptr);
    struct tm local;
    localtime_r(&now, &local);

   
    if (local.tm_sec == last_second)
    {
        return;
    }
    last_second = local.tm_sec;

    char text[32];

    strftime(text, sizeof(text), "%H:%M:%S", &local);
    lv_label_set_text(time_label, text);

    strftime(text, sizeof(text), "%a, %d %b %Y", &local);
    if (strcmp(lv_label_get_text(date_label), text) != 0)
    {
        lv_label_set_text(date_label, text);
    }
}

void LCD::display()
{
    update_clock();

    lv_timer_handler();
}
