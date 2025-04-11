#include "debounced_input.h"

debounced_input_t* debounced_input_new(gpio_num_t gpio, TickType_t debounce) {
    debounced_input_t* input = malloc(sizeof(debounced_input_t));

    input->gpio = gpio;
    input->debounce_const = debounce;
    input->debounce_current = 0;

    return input;
}

void debounced_input_free(debounced_input_t* input) {
    free(input);
}

// Returns 1 if action should happen, 0 if action shouldn't. Also handles debounce updating, so call
// once per frame.
int debounced_input_check(debounced_input_t* input, TickType_t ticks) {
    input->debounce_current = 
        (input->debounce_current > ticks) ? input->debounce_current - ticks : 0;

    // TODO: 1 check may be wrong, may want to make this cofigurable if we accidentally wire
    // buttons differently
    if (gpio_get_level(input->gpio) == 1 && input->debounce_current == 0) {
        input->debounce_current = input->debounce_const;
        return 1;
    }

    return 0;
}
