#include <string.h>
#include <math.h>

#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "debounced_input.h"
#include "ws2815_strip.h"
#include "util.h"

static const char* TAG = "Main";
 
#define FRAME_DURATION_MS 10
#define UART_READ_TIMEOUT_MS 10

#define MODES_NUM 4
#define PALETTE_NUM 10

#define HAPTIC_MOTOR_GPIO 11

#define LED_STRIP_1_GPIO 32
#define LED_STRIP_2_GPIO 14

#define LED_STRIP_1_COUNT 15
#define LED_STRIP_2_COUNT 15

// Function declarations for LED modes
void sync_mode(ws2815_strip_controller_t* strip, uint8_t* rgb_data, uint8_t rgb_count);
void rainbow_mode(ws2815_strip_controller_t* strip, uint16_t offset);
void solid_mode(ws2815_strip_controller_t* strip, const rgb_t* palette, uint8_t palette_idx);
void oscillating_mode(ws2815_strip_controller_t* strip, const rgb_t* palette, uint8_t palette_idx, uint16_t offset);
void strobe_mode(ws2815_strip_controller_t* strip, const rgb_t* palette, uint8_t palette_idx, uint16_t offset);

// Function definitions
void sync_mode(ws2815_strip_controller_t* strip, uint8_t* rgb_data, uint8_t rgb_count) {
    for (int i = 0; (i < ws2815_strip_controller_len(strip) && i < rgb_count); i += 1) {
        ws2815_strip_controller_set(
            strip, 
            i, 
            rgb_data[i * 3], 
            rgb_data[i * 3 + 1], 
            rgb_data[i * 3 + 2]);
    }
}

void rainbow_mode(ws2815_strip_controller_t* strip, uint16_t offset) {
    for (int i = 0; i < ws2815_strip_controller_len(strip); i += 1) {
        rgb_t rgb = hsv2rgb(((5 * i) + offset) % 360, 100.0, 100.0);

        ws2815_strip_controller_set(
            strip, 
            i, 
            rgb.r, 
            rgb.g, 
            rgb.b);
    }
}

void solid_mode(ws2815_strip_controller_t* strip, const rgb_t* palette, uint8_t palette_idx) {
    for (int i = 0; i < ws2815_strip_controller_len(strip); i += 1) {
        rgb_t rgb = palette[palette_idx];

        ws2815_strip_controller_set(
            strip, 
            i, 
            rgb.r, 
            rgb.g, 
            rgb.b);
    }
}

void oscillating_mode(ws2815_strip_controller_t* strip, const rgb_t* palette, uint8_t palette_idx, uint16_t offset) {
    uint16_t offset2 = offset / 5;
    for (int i = 0; i < ws2815_strip_controller_len(strip); i += 1) {
        rgb_t rgb;
        rgb_t rgb2;

        if (offset2 % 2 == 0) {
            rgb = palette[palette_idx];
        } else {
            rgb2 = (rgb_t){.r = 0xff, .g = 0xff, .b = 0xff};
        }

        if (i % 2 == 0) {    
            ws2815_strip_controller_set(
                strip, 
                i, 
                rgb.r, 
                rgb.g, 
                rgb.b);
        } else {
            ws2815_strip_controller_set(
                strip, 
                i, 
                rgb2.r, 
                rgb2.g, 
                rgb2.b);
        }
    }
}

void strobe_mode(ws2815_strip_controller_t* strip, const rgb_t* palette, uint8_t palette_idx, uint16_t offset) {
    uint16_t offset2 = offset / 3;
    for (int i = 0; i < ws2815_strip_controller_len(strip); i += 1) {
        rgb_t rgb = palette[palette_idx];

        if (offset2 % 2 == 0) {    
            ws2815_strip_controller_set(
                strip, 
                i, 
                0x00, 
                0x00, 
                0x00);
        } else {
            ws2815_strip_controller_set(
                strip, 
                i, 
                rgb.r, 
                rgb.g, 
                rgb.b);
        }
    }
}

void init_uart(int uart_num) {
   uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };

    ESP_ERROR_CHECK(uart_driver_install(uart_num, 1024 * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
}
 
void app_main(void) {       
    // UART Initialization
    const uart_port_t uart_num = UART_NUM_0;
    init_uart(uart_num);

    // State variables for reading from desktop
    uint8_t* rgb_data = malloc(sizeof(uint8_t) * 3 * 255); // worst case scenario (all lights)
    uint8_t rgb_count = 0; // THIS COUNTS TRIPLETS! size of valid rgb_data is rgb_count * 3
    uint8_t haptic_motor_trigger = 0;

    // Create LED strips
    ESP_LOGI(
        TAG, 
        "Create LED Strip at GPIO pin %d and GPIO pin %d", 
        LED_STRIP_1_GPIO, 
        LED_STRIP_2_GPIO);

    ws2815_strip_controller_t* strip1 = ws2815_strip_controller_new(
        LED_STRIP_1_GPIO, 
        LED_STRIP_1_COUNT);
    
    ws2815_strip_controller_t* strip2 = ws2815_strip_controller_new(
        LED_STRIP_2_GPIO, 
        LED_STRIP_2_COUNT);

    // Control state
    uint8_t mode = 0; // What LED pattern (solid, oscillating, screen sync, etc...)

    // What the middle button does - picks a 'primary' color from this list that different modes
    // can use
    const rgb_t palette[PALETTE_NUM] = {
        {.r=0xff, .g=0xff, .b=0xff}, // white
        {.r=0xfc, .g=0xf4, .b=0x00}, // yellow
        {.r=0xff, .g=0x64, .b=0x00}, // orange
        {.r=0xdd, .g=0x02, .b=0x02}, // red
        {.r=0xf0, .g=0x02, .b=0x85}, // magenta
        {.r=0x46, .g=0x00, .b=0xa5}, // purple
        {.r=0x00, .g=0x00, .b=0xd5}, // blue
        {.r=0x00, .g=0xae, .b=0xe9}, // cyan
        {.r=0x1a, .g=0xb9, .b=0x0c}, // green
        {.r=0x00, .g=0x64, .b=0x08}, // dark green
    };
    uint8_t palette_idx = 8; 
    uint16_t offset = 0; // for rainbow and oscillating effects

    debounced_input_t* left_button = debounced_input_new(GPIO_NUM_10, pdMS_TO_TICKS(200));
    debounced_input_t* right_button = debounced_input_new(GPIO_NUM_11, pdMS_TO_TICKS(200));
    debounced_input_t* mid_button = debounced_input_new(GPIO_NUM_12, pdMS_TO_TICKS(200));


    uint8_t header[6] = {0};
    // "ARCADE" in ascii
    const uint8_t expected[6] = {0x41, 0x52, 0x43, 0x41, 0x44, 0x45};     

    while (1) {
        TickType_t ticks = pdMS_TO_TICKS(FRAME_DURATION_MS);

        // This shifty thing is good for error checking but may introduce to much latency for haptic
        // motor + LED sync idk (try lowering frame_duration_ms super low first)
        for (int i = 0; i < 5; i += 1) {
            header[i] = header[i + 1];
        }

        uint8_t new_byte;
        if (uart_read_bytes(uart_num, &new_byte, 1, pdMS_TO_TICKS(UART_READ_TIMEOUT_MS)) == 1) {
            header[5] = new_byte;
        
            int header_match = 1;
            for (int i = 0; i < 6; i += 1) {
                if (header[i] != expected[i]) {
                    header_match = 0;
                    break;
                }
            }

            if (header_match) {
                // Assume these all succeed (we have header match)
                uart_read_bytes(uart_num, &haptic_motor_trigger, 1, pdMS_TO_TICKS(UART_READ_TIMEOUT_MS));
                uart_read_bytes(uart_num, &rgb_count, 1, pdMS_TO_TICKS(UART_READ_TIMEOUT_MS));
                uart_read_bytes(uart_num, rgb_data, rgb_count * 3, pdMS_TO_TICKS(UART_READ_TIMEOUT_MS));
            }
        }

        // Handle button input and (TODO: LCD logic)
        /**if (debounced_input_check(left_button, ticks)) {
            mode = ((mode - 1) + MODES_NUM) % MODES_NUM;
            ESP_LOGI(TAG, "Left button pressed");
        }

        if (debounced_input_check(right_button, ticks)) {
            mode = (mode + 1) % MODES_NUM;
            ESP_LOGI(TAG, "Right button pressed");
        }

        // mode > 1: only have mid button work on the modes where it changes things
        if (debounced_input_check(mid_button, ticks) && mode > 1) {
            ESP_LOGI(TAG, "Oh yeah!!!!");
        }**/
        

        /**
         * LED Modes:
         * Mode 0: Sync 
         * Mode 1: Rainbow
         * Mode 2: Solid
         * Mode 3: Oscillating
         * Mode 4: Strobe
         */

        if (mode == 0) {
            sync_mode(strip1, rgb_data, rgb_count);
            sync_mode(strip2, rgb_data, rgb_count);
        } else if (mode == 1) {
            rainbow_mode(strip1, offset);
            rainbow_mode(strip2, offset);
        } else if (mode == 2) {
            solid_mode(strip1, palette, palette_idx);
            solid_mode(strip2, palette, palette_idx);
        } else if (mode == 3) {
            oscillating_mode(strip1, palette, palette_idx, offset);
            oscillating_mode(strip2, palette, palette_idx, offset);
        } else if (mode == 4) {
            strobe_mode(strip1, palette, palette_idx, offset);
            strobe_mode(strip2, palette, palette_idx, offset);
        }

        ws2815_strip_controller_send(strip1);
        ws2815_strip_controller_send(strip2);
        
        gpio_set_level(HAPTIC_MOTOR_GPIO, (uint32_t)haptic_motor_trigger);

        // You are not paranoid enough. Doing the modulo for overflow
        offset = (offset + 1) % 65536;

        vTaskDelay(ticks);
    }
}