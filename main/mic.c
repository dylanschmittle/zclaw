#include "mic.h"

#if ZCLAW_HAS_MICROPHONE

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "driver/i2s_pdm.h"

static const char *TAG = "mic";

static i2s_chan_handle_t s_rx_chan = NULL;
static bool s_initialized = false;

esp_err_t mic_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "mic already initialized");
        return ESP_OK;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &s_rx_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s channel create failed: %s", esp_err_to_name(err));
        return err;
    }

    i2s_pdm_rx_config_t pdm_cfg = {
        .clk_cfg  = I2S_PDM_RX_CLK_DEFAULT_CONFIG(MIC_SAMPLE_RATE),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = MIC_PIN_CLK,
            .din = MIC_PIN_DATA,
            .invert_flags = { .clk_inv = false },
        },
    };

    err = i2s_channel_init_pdm_rx_mode(s_rx_chan, &pdm_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s pdm rx init failed: %s", esp_err_to_name(err));
        i2s_del_channel(s_rx_chan);
        s_rx_chan = NULL;
        return err;
    }

    err = i2s_channel_enable(s_rx_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s channel enable failed: %s", esp_err_to_name(err));
        i2s_del_channel(s_rx_chan);
        s_rx_chan = NULL;
        return err;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "mic initialized (%d Hz, %d-bit mono)", MIC_SAMPLE_RATE, MIC_SAMPLE_BITS);
    return ESP_OK;
}

bool mic_record(uint32_t duration_ms, int16_t **buf, size_t *len)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "mic not initialized");
        return false;
    }

    if (duration_ms > (uint32_t)(MIC_RECORD_SECS_MAX * 1000)) {
        duration_ms = MIC_RECORD_SECS_MAX * 1000;
        ESP_LOGW(TAG, "clamped recording to %d ms", (int)duration_ms);
    }

    size_t total_samples = (MIC_SAMPLE_RATE * duration_ms) / 1000;
    size_t total_bytes = total_samples * sizeof(int16_t);

#if ZCLAW_HAS_PSRAM
    int16_t *audio_buf = heap_caps_malloc(total_bytes, MALLOC_CAP_SPIRAM);
#else
    int16_t *audio_buf = malloc(total_bytes);
#endif

    if (!audio_buf) {
        ESP_LOGE(TAG, "failed to allocate %u bytes for audio", (unsigned)total_bytes);
        return false;
    }

    size_t bytes_read = 0;
    size_t offset = 0;
    size_t chunk = 1024;

    while (offset < total_bytes) {
        size_t to_read = total_bytes - offset;
        if (to_read > chunk) {
            to_read = chunk;
        }
        esp_err_t err = i2s_channel_read(s_rx_chan, (uint8_t *)audio_buf + offset,
                                          to_read, &bytes_read, pdMS_TO_TICKS(1000));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "i2s read failed: %s", esp_err_to_name(err));
            free(audio_buf);
            return false;
        }
        offset += bytes_read;
    }

    *buf = audio_buf;
    *len = offset;
    ESP_LOGI(TAG, "recorded %u bytes (%u ms)", (unsigned)offset, (unsigned)duration_ms);
    return true;
}

void mic_deinit(void)
{
    if (!s_initialized) {
        return;
    }
    i2s_channel_disable(s_rx_chan);
    i2s_del_channel(s_rx_chan);
    s_rx_chan = NULL;
    s_initialized = false;
    ESP_LOGI(TAG, "mic deinitialized");
}

#endif // ZCLAW_HAS_MICROPHONE
