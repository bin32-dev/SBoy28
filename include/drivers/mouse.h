#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int16_t dx;
    int16_t dy;
    bool left;
    bool right;
    bool middle;
} mouse_event_t;

typedef void (*mouse_event_handler_t)(mouse_event_t *event);

void mouse_init(void);
void mouse_register_handler(mouse_event_handler_t handler);
void mouse_get_position(int32_t *x, int32_t *y);
void mouse_get_buttons(bool* left, bool* right, bool* middle);
bool mouse_consume_left_click(void);
bool mouse_poll(mouse_event_t *out);

#endif
