#ifndef SBOY28_OS_GUI_H
#define SBOY28_OS_GUI_H

#include <stdbool.h>
#include <stdint.h>

#include "drivers/vga.h"

typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} os_rect_t;

void os_gui_fill_rect(os_rect_t rect, uint8_t color);
void os_gui_draw_rect_outline(os_rect_t rect, uint8_t color);
void os_gui_draw_title_bar(os_rect_t rect, const char* title);
void os_gui_draw_desktop_background(void);
void os_gui_draw_taskbar(bool start_menu_open);
void os_gui_draw_window(os_rect_t rect, const char* title);
void os_gui_draw_cursor(int32_t x, int32_t y, bool left_pressed);
bool os_gui_point_in_rect(int32_t x, int32_t y, os_rect_t rect);

#endif
