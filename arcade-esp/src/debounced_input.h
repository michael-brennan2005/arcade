#ifndef DEBOUNCED_INPUT_H
#define DEBOUNCED_INPUT_H

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"

/**
 * Simple wrapper for a debounced button. Input is read from GPIO line (1 = button pressed,
 * 0 = button not pressed)
 */
typedef struct {
    gpio_num_t gpio;
    TickType_t debounce_current;
    TickType_t debounce_const;
} debounced_input_t;

debounced_input_t* debounced_input_new(gpio_num_t gpio, TickType_t debounce);
void debounced_input_free(debounced_input_t* input);

// Returns 1 if action should happen, 0 if action shouldn't. Also handles debounce updating, so call
// once per frame.
int debounced_input_check(debounced_input_t* input, TickType_t ticks);

#endif