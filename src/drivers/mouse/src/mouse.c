/*
 * PS/2 mouse driver (3-byte packet parser)
 *
 * Assumptions:
 * - Kernel runs in protected mode, PIC already remapped.
 * - IRQ12 is routed and the assembly wrapper calls mouse_irq_handler().
 * - outb/inb primitives exist below (included here for portability).
 *
 * Drop into kernel/driver/input/mouse/.
 */


#include "common/ports.h"
#include "kernel/idt.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <drivers/mouse.h>

/* I/O ports */
#define PS2_DATA_PORT  0x60
#define PS2_STATUS_REG 0x64
#define PS2_CMD_PORT   0x64

/* PS/2 controller commands */
#define PS2_ENABLE_AUX 0xA8
#define PS2_WRITE_COMPAQ 0xD4
#define PS2_ENABLE_STREAM 0xF4
#define PS2_SET_DEFAULTS 0xF6
#define PS2_RESET       0xFF

/* Status flags */
#define PS2_STATUS_OUTPUT_FULL (1 << 0)
#define PS2_STATUS_INPUT_FULL  (1 << 1)
#define PS2_STATUS_AUXDATA     (1 << 5)

/* small millisecond delay loop (for controller timing) */
static inline void io_wait(void) {
    outb(0x80, 0);
}

/* Wait until controller input buffer is clear */
static void wait_input_clear(void) {
    uint32_t i = 0;
    while (inb(PS2_STATUS_REG) & PS2_STATUS_INPUT_FULL) {
        if (++i > 100000) break;
    }
}

/* Wait until output buffer has data */
static uint8_t wait_for_output(void) {
    uint32_t i = 0;
    while (!(inb(PS2_STATUS_REG) & PS2_STATUS_OUTPUT_FULL)) {
        if (++i > 100000) return 0; /* timeout */
    }
    return inb(PS2_DATA_PORT);
}

/* Send a command to the PS/2 auxiliary device (mouse) */
static void ps2_send_to_aux(uint8_t byte) {
    wait_input_clear();
    outb(PS2_CMD_PORT, PS2_WRITE_COMPAQ); /* tell controller next byte is for aux device */
    io_wait();
    wait_input_clear();
    outb(PS2_DATA_PORT, byte);
}

/* Driver state */
static uint8_t packet[4];
static int packet_index = 0; /* expecting 0..2 (or 3 for 4-byte packets if implemented) */
static bool has_handler = false;
static mouse_event_handler_t event_handler = NULL;

/* Graphical coordinates */
static int32_t mouse_x = 160;
static int32_t mouse_y = 100;
static bool mouse_left_pressed = false;
static bool mouse_right_pressed = false;
static bool mouse_middle_pressed = false;
static bool mouse_left_click_pending = false;
#define MOUSE_SCREEN_W 320
#define MOUSE_SCREEN_H 200

/* Helper: sign-extend 8-bit to int16 for deltas */
static inline int16_t s8_to_s16(uint8_t v) {
    return (int16_t)(int8_t)v;
}

/* Publish parsed event to registered handler (if any) */
static void publish_event(int16_t dx, int16_t dy, bool l, bool r, bool m) {
    if (!has_handler || event_handler == NULL) return;
    mouse_event_t evt = {
        .dx = dx,
        .dy = dy,
        .left = l,
        .right = r,
        .middle = m
    };
    event_handler(&evt);
}

/* Public API */
void mouse_register_handler(mouse_event_handler_t handler) {
    event_handler = handler;
    has_handler = (handler != NULL);
}

void mouse_get_position(int32_t *x, int32_t *y) {
    if (x) *x = mouse_x;
    if (y) *y = mouse_y;
}

void mouse_get_buttons(bool* left, bool* right, bool* middle) {
    if (left) *left = mouse_left_pressed;
    if (right) *right = mouse_right_pressed;
    if (middle) *middle = mouse_middle_pressed;
}

bool mouse_consume_left_click(void) {
    bool clicked = mouse_left_click_pending;
    mouse_left_click_pending = false;
    return clicked;
}

/* Basic polling read (safe to call from non-IRQ contexts) */
bool mouse_poll(mouse_event_t *out) {
    /* Read one packet if available. Not robust — prefer IRQ flow. */
    if (!(inb(PS2_STATUS_REG) & PS2_STATUS_OUTPUT_FULL)) return false;
    uint8_t b = inb(PS2_DATA_PORT);
    packet[packet_index++] = b;
    if (packet_index >= 3) {
        packet_index = 0;
        int16_t dx = s8_to_s16(packet[1]);
        int16_t dy = -s8_to_s16(packet[2]); /* PS/2 dy: positive = up? invert if UI expects otherwise */
        bool l = packet[0] & 0x1;
        bool r = packet[0] & 0x2;
        bool m = packet[0] & 0x4;

        if (l && !mouse_left_pressed) {
            mouse_left_click_pending = true;
        }
        mouse_left_pressed = l;
        mouse_right_pressed = r;
        mouse_middle_pressed = m;

        if (out) {
            out->dx = dx;
            out->dy = dy;
            out->left = l;
            out->right = r;
            out->middle = m;
        }
        return true;
    }
    return false;
}

/* High-level device-specific init sequence */
static void mouse_device_init(void) {
    /* Send command 0xF6: Set Defaults */
    ps2_send_to_aux(PS2_SET_DEFAULTS);
    wait_for_output(); /* ACK (0xFA) */
    io_wait();

    /* Send command 0xF4: Enable Streaming */
    ps2_send_to_aux(PS2_ENABLE_STREAM);
    wait_for_output(); /* ACK (0xFA) */
    io_wait();

    /* Flush any stale data left in the buffer */
    while (inb(PS2_STATUS_REG) & PS2_STATUS_OUTPUT_FULL) {
        (void)inb(PS2_DATA_PORT);
    }
}

/* IRQ handler called from IDT (context: IRQ12) */
static void mouse_irq_handler(registers_t *regs) {
    (void)regs;
    /* Read a byte. If no data, return. */
    if (!(inb(PS2_STATUS_REG) & PS2_STATUS_OUTPUT_FULL)) {
        return;
    }
    uint8_t data = inb(PS2_DATA_PORT);

    /* 
     * Handle BAT and ACK sequences:
     * 0xAA (BAT OK) followed by 0x00 (ID) means the mouse was just plugged in or reset.
     * 0xFA is an ACK from a command.
     */
    if (data == 0xAA) {
        /* BAT OK received, reset state and wait for ID (0x00) */
        packet_index = 0;
        return;
    } else if (data == 0x00 && packet_index == 0) {
        /* BAT Device ID received, need to re-initialize streaming */
        mouse_device_init();
        return;
    } else if (data == 0xFA) {
        /* ACK received - ignore if it shows up in streaming */
        return;
    }

    packet[packet_index++] = data;

    if (packet_index >= 3) {
        packet_index = 0;
        /* Validate packet: bit 3 of first byte should be 1 (always) */
        if (!(packet[0] & 0x08)) {
            /* bad sync, discard and try to re-sync next byte */
            return;
        }
        int16_t dx = s8_to_s16(packet[1]);
        int16_t dy = -s8_to_s16(packet[2]); /* invert Y to match typical screen coords */
        bool l = packet[0] & 0x1;
        bool r = packet[0] & 0x2;
        bool m = packet[0] & 0x4;

        if (l && !mouse_left_pressed) {
            mouse_left_click_pending = true;
        }

        mouse_left_pressed = l;
        mouse_right_pressed = r;
        mouse_middle_pressed = m;

        /* Update global coordinates */
        mouse_x += dx;
        mouse_y += dy;

        /* Clamp to screen boundaries */
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_x >= MOUSE_SCREEN_W) mouse_x = MOUSE_SCREEN_W - 1;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_y >= MOUSE_SCREEN_H) mouse_y = MOUSE_SCREEN_H - 1;

        publish_event(dx, dy, l, r, m);
    }
}

/* High-level init sequence */
void mouse_init(void) {
    /* enable auxiliary device (mouse) */
    wait_input_clear();
    outb(PS2_CMD_PORT, PS2_ENABLE_AUX);
    io_wait();

    /* Enable IRQ12 in the PS/2 controller configuration byte */
    wait_input_clear();
    outb(PS2_CMD_PORT, 0x20); // Read Controller Config Byte
    io_wait();
    uint8_t status = wait_for_output();

    status |= (1 << 1); // Enable IRQ12 (bit 1)
    
    wait_input_clear();
    outb(PS2_CMD_PORT, 0x60); // Write Controller Config Byte
    io_wait();
    wait_input_clear();
    outb(PS2_DATA_PORT, status);
    io_wait();

    /* driver state */
    packet_index = 0;
    has_handler = false;
    event_handler = NULL;
    mouse_left_pressed = false;
    mouse_right_pressed = false;
    mouse_middle_pressed = false;
    mouse_left_click_pending = false;

    /* device specific init */
    mouse_device_init();

    /* Install IRQ handler for IRQ12 (Interrupt 44) */
    register_interrupt_handler(44, mouse_irq_handler);
}
