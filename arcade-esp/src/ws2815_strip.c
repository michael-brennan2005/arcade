#include <stdio.h>

#include "esp_log.h"
#include "driver/rmt_tx.h"

#include "ws2815_strip.h"

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000

static const char* TAG = "WS2815_Strip";
static const rmt_transmit_config_t tx_config = {
    .loop_count = 0
};

static const rmt_symbol_word_t ws2812_zero = {
    .level0 = 1,
    .duration0 = 0.3 * RMT_LED_STRIP_RESOLUTION_HZ / 1000000, // T0H=0.3us
    .level1 = 0,
    .duration1 = 1 * RMT_LED_STRIP_RESOLUTION_HZ / 1000000, // T0L=1us
};

static const rmt_symbol_word_t ws2812_one = {
    .level0 = 1,
    .duration0 = 1 * RMT_LED_STRIP_RESOLUTION_HZ / 1000000, // T1H=1us
    .level1 = 0,
    .duration1 = 0.3 * RMT_LED_STRIP_RESOLUTION_HZ / 1000000, // T1L=0.3us
};

//reset defaults to 280uS
static const rmt_symbol_word_t ws2812_reset = {
    .level0 = 0,
    .duration0 = RMT_LED_STRIP_RESOLUTION_HZ / 1000000 * 300 / 2,
    .level1 = 0,
    .duration1 = RMT_LED_STRIP_RESOLUTION_HZ / 1000000 * 300 / 2,
};

// NOTE (cause this confuses the shit out of me):
// - Our data is RGB triplets (3x uint8_t)
// - RMT takes symbols (which have a duration and signal (HI/LO)).
// - This callback is what handles that conversion.
// TODO: Because this converts data into a byte pointer I think we have nothing XTRA do to convert it?
// Could be VERY VERY wrong about this tho
// UPDATE TO TODO: Yeah this is chill
static size_t encoder_callback(const void *data, size_t data_size,
                               size_t symbols_written, size_t symbols_free,
                               rmt_symbol_word_t *symbols, bool *done, void *arg) {
    // We need a minimum of 8 symbol spaces to encode a byte. We only
    // need one to encode a reset, but it's simpler to simply demand that
    // there are 8 symbol spaces free to write anything.
    if (symbols_free < 8) {
        return 0;
    }

    // We can calculate where in the data we are from the symbol pos.
    // Alternatively, we could use some counter referenced by the arg
    // parameter to keep track of this.
    size_t data_pos = symbols_written / 8;
    uint8_t *data_bytes = (uint8_t*)data;
    if (data_pos < data_size) {
        // Encode a byte
        size_t symbol_pos = 0;
        for (int bitmask = 0x80; bitmask != 0; bitmask >>= 1) {
            if (data_bytes[data_pos]&bitmask) {
                symbols[symbol_pos++] = ws2812_one;
            } else {
                symbols[symbol_pos++] = ws2812_zero;
            }
        }
        // We're done; we should have written 8 symbols.
        return symbol_pos;
    } else {
        //All bytes already are encoded.
        //Encode the reset, and we're done.
        symbols[0] = ws2812_reset;
        *done = 1; //Indicate end of the transaction.
        return 1; //we only wrote one symbol
    }
}

// TODO: shitty error handling (should we adopt esp_err_t's everywhere or do someting else)
ws2815_strip_controller_t* ws2815_strip_controller_new(gpio_num_t gpio_num, uint32_t strip_length) {
    ESP_LOGI(TAG, "Creating WS2815_Strip, Len %d at GPIO %d", (int)strip_length, (int)gpio_num);

    ws2815_strip_controller_t* controller = malloc(sizeof(ws2815_strip_controller_t));
    controller->buf = malloc(strip_length * 3); // r,g,b is a byte each
    controller->len = strip_length;

    rmt_channel_handle_t channel = NULL;
    rmt_tx_channel_config_t channel_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio_num,
        .mem_block_symbols = strip_length * 3 * 8, // one symbol for every bit (TODO: is this right at all)  
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4,
    };

    ESP_ERROR_CHECK(rmt_new_tx_channel(&channel_config, &channel));

    rmt_encoder_handle_t encoder = NULL;
    // KISS
    const rmt_simple_encoder_config_t encoder_cfg = {
        .callback = encoder_callback
    };

    // TODO: does every channel need its own identical encoder
    ESP_ERROR_CHECK(rmt_new_simple_encoder(&encoder_cfg, &encoder)); 
    
    // const rmt_bytes_encoder_config_t encoder_config = {
    //     .bit0 = ws2812_zero,
    //     .bit1 = ws2812_one,
    //     .flags = {
    //         .msb_first = 0
    //     }
    // };

    // ESP_ERROR_CHECK(rmt_new_bytes_encoder(&encoder_config, &encoder));
    ESP_ERROR_CHECK(rmt_enable(channel));

    controller->channel = channel;
    controller->encoder = encoder;
    return controller;
};

void ws2815_strip_controller_free(ws2815_strip_controller_t* controller) {
    // Do we need this method lol
    return;
}

int ws2815_strip_controller_len(ws2815_strip_controller_t* controller) {
    return controller->len;
}

void ws2815_strip_controller_set(
    ws2815_strip_controller_t* controller, 
    uint32_t idx, 
    uint8_t r, 
    uint8_t g, 
    uint8_t b) {    
    // From datasheet: 24Bit data is GRB order.
    controller->buf[idx * 3] = g;
    controller->buf[idx * 3 + 1] = r;
    controller->buf[idx * 3 + 2] = b;
    return;
}

void ws2815_strip_controller_send(ws2815_strip_controller_t* controller) {
    ESP_ERROR_CHECK(rmt_transmit(
        controller->channel,
        controller->encoder,
        controller->buf,
        sizeof(uint8_t) * controller->len * 3,
        &tx_config
    ));
}
