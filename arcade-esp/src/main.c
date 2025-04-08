#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "driver/uart.h"

#include "debounced_input.h"
#include "ws2815_strip.h"
#include "util.h"

static const char* TAG = "Main";
 
#define FRAME_DURATION_MS 10


#define MODES_NUM 3

void app_main(void) {       
    // UART for reading LED data
    const uart_port_t uart_num = UART_NUM_0;

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
    // don't set pins here cause its already configured thru USB/UART Bridge

    // Data for reading from UART
    uint8_t* data = malloc(sizeof(uint8_t) * 256);
    int len = 0;

    // Create LED strips
    gpio_num_t led_io_pin = GPIO_NUM_21;
    ESP_LOGI(TAG, "Create LED Strip at GPIO PIN %d", led_io_pin);
    ws2815_strip_controller_t* led_strip = ws2815_strip_controller_new(led_io_pin, 14);

    // LED state
    int mode = 0;
    rgb_t mode_primary;

    debounced_input_t* left_button = debounced_input_new(GPIO_NUM_10, 200);
    debounced_input_t* right_button = debounced_input_new(GPIO_NUM_11, 200);
    debounced_input_t* mid_button = debounced_input_new(GPIO_NUM_12, 200);
    
    while (1) {
        TickType_t ticks = pdMS_TO_TICKS(FRAME_DURATION_MS);
        
        // Read incoming UART data
        len = uart_read_bytes(uart_num, data, 1, 250);
        if (len == 1 && data[0] == 0x77) { // TODO: what if the color starts with 0x77
            uart_read_bytes(uart_num, data, 14 * 3, 250);
        }

        // Handle button input and (TODO: LCD logic)
        if (debounced_input_check(left_button, ticks)) {
            mode = ((mode - 1) + MODES_NUM) % MODES_NUM;
            ESP_LOGI(TAG, "Left button pressed");
        }

        if (debounced_input_check(right_button, ticks)) {
            mode = (mode + 1) % MODES_NUM;
            ESP_LOGI(TAG, "Right button pressed");
        }

        if (debounced_input_check(mid_button, ticks)) {
            ESP_LOGI(TAG, "Oh yeah!!!!");
        }

        if (MODES_NUM == 0) {
            for (int i = 0; i < ws2815_strip_controller_len(led_strip); i += 1) {
                ws2815_strip_controller_set(
                    led_strip, 
                    i, 
                    data[i * 3], 
                    data[i * 3 + 1], 
                    data[i * 3 + 2]);
            }
        } else if (MODES_NUM == 1) {
            for (int i = 0; i < ws2815_strip_controller_len(led_strip); i += 1) {
                ws2815_strip_controller_set(
                    led_strip, 
                    i, 
                    0, 
                    255, 
                    0);
            }

        } else if (MODES_NUM == 2) {
            for (int i = 0; i < ws2815_strip_controller_len(led_strip); i += 1) {
                ws2815_strip_controller_set(
                    led_strip, 
                    i, 
                    255, 
                    0, 
                    0);
            }
        }
        ws2815_strip_controller_send(led_strip);
        
        TickType_t ticks = pdMS_TO_TICKS(FRAME_DURATION_MS);
        vTaskDelay(ticks);
    }
}