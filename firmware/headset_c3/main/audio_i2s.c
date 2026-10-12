#include "audio_i2s.h"

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "hs_i2s";

static i2s_chan_handle_t tx_chan;
static i2s_chan_handle_t rx_chan;

bool audio_i2s_start(void) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 6;
    chan_cfg.dma_frame_num = 240;
    if (i2s_new_channel(&chan_cfg, &tx_chan, &rx_chan) != ESP_OK) {
        ESP_LOGE(TAG, "sem canal I2S");
        return false;
    }

    i2s_std_slot_config_t slot = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
    slot.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(48000),
        .slot_cfg = slot,
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_2,
            .ws = GPIO_NUM_3,
            .dout = GPIO_NUM_5,
            .din = GPIO_NUM_4,
            .invert_flags = {0},
        },
    };
    if (i2s_channel_init_std_mode(tx_chan, &std_cfg) != ESP_OK ||
        i2s_channel_init_std_mode(rx_chan, &std_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "I2S nao configurou");
        return false;
    }
    i2s_channel_enable(tx_chan);
    i2s_channel_enable(rx_chan);
    gpio_pullup_en(GPIO_NUM_2);
    return true;
}

bool audio_i2s_read_left(int16_t *left, size_t frames) {
    int32_t raw[240 * 2];
    if (frames > 240) {
        frames = 240;
    }
    size_t bytes = 0;
    esp_err_t err = i2s_channel_read(rx_chan, raw, frames * 2 * sizeof(int32_t), &bytes, pdMS_TO_TICKS(20));
    size_t got = bytes / sizeof(int32_t) / 2;
    for (size_t i = 0; i < got; i++) {
        left[i] = (int16_t)(raw[i * 2] >> 16);
    }
    for (size_t i = got; i < frames; i++) {
        left[i] = 0;
    }
    return err == ESP_OK && got == frames;
}

void audio_i2s_write_stereo(const int16_t *interleaved, size_t frames) {
    int32_t raw[240 * 2];
    if (frames > 240) {
        frames = 240;
    }
    for (size_t i = 0; i < frames; i++) {
        raw[i * 2] = ((int32_t)interleaved[i * 2]) << 16;
        raw[i * 2 + 1] = ((int32_t)interleaved[i * 2 + 1]) << 16;
    }
    size_t written = 0;
    i2s_channel_write(tx_chan, raw, frames * 2 * sizeof(int32_t), &written, pdMS_TO_TICKS(20));
}
