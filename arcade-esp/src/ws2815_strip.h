#ifndef WS2815_STRIP_H
#define WS2815_STRIP_H

#include <driver/rmt_types.h>

/**
 * Mode of operation:
 * 1. Create controller with specified led length.
 * 2. Use len and buf getters to set what the colors you want for LEDs.
 * 3. Call ws2815_strip_controller_send to do the RMT logic and send signals.
 * 
 * TODO: how to handle reset times.
 */
typedef struct {
    uint8_t* buf;
    // STRIP LENGTH! sizeof(buf) = sizeof(uint8)*len*3; This is lowkey a bad name.
    uint32_t len; 
    rmt_channel_handle_t channel;
    rmt_encoder_handle_t encoder;
} ws2815_strip_controller_t;

ws2815_strip_controller_t* ws2815_strip_controller_new(gpio_num_t gpio_num, uint32_t strip_length);
void ws2815_strip_controller_free(ws2815_strip_controller_t* controller);

int ws2815_strip_controller_len(ws2815_strip_controller_t* controller);
void ws2815_strip_controller_set(
    ws2815_strip_controller_t* controller, 
    uint32_t idx, 
    uint8_t r, 
    uint8_t g, 
    uint8_t b);

void ws2815_strip_controller_send(ws2815_strip_controller_t* controller);

#endif