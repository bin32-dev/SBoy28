#include "OS/gui.h"
#include "ui/colors.h"

void os_gui_fill_rect(os_rect_t rect, uint8_t color)
{
    for (int32_t y = rect.y; y < rect.y + rect.h; y++) {
        for (int32_t x = rect.x; x < rect.x + rect.w; x++) {
            vga_put_pixel((uint32_t)x, (uint32_t)y, color);
        }
    }
}

void os_gui_draw_rect_outline(os_rect_t rect, uint8_t color)
{
    for (int32_t x = rect.x; x < rect.x + rect.w; x++) {
        vga_put_pixel((uint32_t)x, (uint32_t)rect.y, color);
        vga_put_pixel((uint32_t)x, (uint32_t)(rect.y + rect.h - 1), color);
    }

    for (int32_t y = rect.y; y < rect.y + rect.h; y++) {
        vga_put_pixel((uint32_t)rect.x, (uint32_t)y, color);
        vga_put_pixel((uint32_t)(rect.x + rect.w - 1), (uint32_t)y, color);
    }
}

void os_gui_draw_title_bar(os_rect_t rect, const char* title)
{
    os_rect_t title_bar = { rect.x + 1, rect.y + 1, rect.w - 2, 10 };
    os_gui_fill_rect(title_bar, BRIGHT_BLUE);
    vga_draw_string((uint32_t)(rect.x + 4), (uint32_t)(rect.y + 2), title, WHITE);
}

void os_gui_draw_desktop_background(void)
{
    os_rect_t sky = { 0, 0, VGA_WIDTH, VGA_HEIGHT - 16 };
    os_gui_fill_rect(sky, CYAN);

    os_rect_t ground = { 0, 120, VGA_WIDTH, VGA_HEIGHT - 136 };
    os_gui_fill_rect(ground, GREEN);
}

void os_gui_draw_taskbar(bool start_menu_open)
{
    os_rect_t bar = { 0, VGA_HEIGHT - 16, VGA_WIDTH, 16 };
    os_gui_fill_rect(bar, DARK_GREY);

    os_rect_t start_button = { 4, VGA_HEIGHT - 14, 46, 12 };
    os_gui_fill_rect(start_button, start_menu_open ? BRIGHT_CYAN : GREY);
    os_gui_draw_rect_outline(start_button, WHITE);
    vga_draw_string(10, VGA_HEIGHT - 12, "Start", BLACK);
}

void os_gui_draw_window(os_rect_t rect, const char* title)
{
    os_gui_fill_rect(rect, LIGHT_GREY);
    os_gui_draw_rect_outline(rect, WHITE);

    os_rect_t shadow = { rect.x + rect.w, rect.y + 2, 2, rect.h };
    os_gui_fill_rect(shadow, DARK_GREY);

    os_gui_draw_title_bar(rect, title);
}

void os_gui_draw_cursor(int32_t x, int32_t y, bool left_pressed)
{
    uint8_t color = left_pressed ? BRIGHT_RED : WHITE;

    for (int32_t dy = 0; dy < 8; dy++) {
        vga_put_pixel((uint32_t)x, (uint32_t)(y + dy), color);
    }

    for (int32_t dx = 0; dx < 6; dx++) {
        vga_put_pixel((uint32_t)(x + dx), (uint32_t)y, color);
    }

    vga_put_pixel((uint32_t)(x + 1), (uint32_t)(y + 1), color);
    vga_put_pixel((uint32_t)(x + 2), (uint32_t)(y + 2), color);
    vga_put_pixel((uint32_t)(x + 3), (uint32_t)(y + 3), color);
}

bool os_gui_point_in_rect(int32_t x, int32_t y, os_rect_t rect)
{
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}
