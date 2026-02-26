#include "tools_media.h"
#include "tools_handlers.h"
#include "config.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if ZCLAW_HAS_CAMERA
#include "camera.h"
#endif
#if ZCLAW_HAS_MICROPHONE
#include "mic.h"
#endif

static const char *TAG = "media";

// Base64 encoding table
static const char s_b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Pending image state
static char *s_pending_b64 = NULL;
static size_t s_pending_b64_len = 0;
static char s_pending_tool_id[64] = {0};

// Base64-encode binary data into a malloc'd string.
// Returns allocated string (caller must free) or NULL on failure.
static char *base64_encode(const uint8_t *data, size_t data_len, size_t *out_len)
{
    size_t encoded_len = 4 * ((data_len + 2) / 3);

#if ZCLAW_HAS_PSRAM
    char *out = heap_caps_malloc(encoded_len + 1, MALLOC_CAP_SPIRAM);
#else
    char *out = malloc(encoded_len + 1);
#endif

    if (!out) {
        return NULL;
    }

    size_t i = 0;
    size_t j = 0;

    while (i + 2 < data_len) {
        uint32_t triple = ((uint32_t)data[i] << 16) |
                          ((uint32_t)data[i + 1] << 8) |
                          (uint32_t)data[i + 2];
        out[j++] = s_b64_table[(triple >> 18) & 0x3F];
        out[j++] = s_b64_table[(triple >> 12) & 0x3F];
        out[j++] = s_b64_table[(triple >> 6) & 0x3F];
        out[j++] = s_b64_table[triple & 0x3F];
        i += 3;
    }

    if (i < data_len) {
        uint32_t a = data[i];
        uint32_t b = (i + 1 < data_len) ? data[i + 1] : 0;

        out[j++] = s_b64_table[(a >> 2) & 0x3F];
        if (i + 1 < data_len) {
            out[j++] = s_b64_table[((a & 0x03) << 4) | ((b >> 4) & 0x0F)];
            out[j++] = s_b64_table[(b & 0x0F) << 2];
        } else {
            out[j++] = s_b64_table[(a & 0x03) << 4];
            out[j++] = '=';
        }
        out[j++] = '=';
    }

    out[j] = '\0';
    if (out_len) {
        *out_len = j;
    }
    return out;
}

void media_init(void)
{
#if ZCLAW_HAS_CAMERA
    esp_err_t err = camera_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(err));
    }
#endif

#if ZCLAW_HAS_MICROPHONE
    esp_err_t mic_err = mic_init();
    if (mic_err != ESP_OK) {
        ESP_LOGE(TAG, "Mic init failed: %s", esp_err_to_name(mic_err));
    }
#endif

    (void)TAG;
}

bool media_has_pending_image(void)
{
    return s_pending_b64 != NULL;
}

bool media_get_pending_image(const char **base64_out, size_t *base64_len_out,
                             const char **tool_id_out)
{
    if (!s_pending_b64) {
        return false;
    }

    if (base64_out) {
        *base64_out = s_pending_b64;
    }
    if (base64_len_out) {
        *base64_len_out = s_pending_b64_len;
    }
    if (tool_id_out) {
        *tool_id_out = s_pending_tool_id;
    }
    return true;
}

void media_set_pending_tool_id(const char *tool_id)
{
    if (tool_id) {
        strncpy(s_pending_tool_id, tool_id, sizeof(s_pending_tool_id) - 1);
        s_pending_tool_id[sizeof(s_pending_tool_id) - 1] = '\0';
    }
}

void media_release_pending(void)
{
    if (s_pending_b64) {
        free(s_pending_b64);
        s_pending_b64 = NULL;
        s_pending_b64_len = 0;
    }
    s_pending_tool_id[0] = '\0';

#if ZCLAW_HAS_CAMERA
    camera_release_frame();
#endif
}

// ---------------------------------------------------------------------------
// capture_photo tool handler
// ---------------------------------------------------------------------------
#if ZCLAW_HAS_CAMERA

bool tools_capture_photo_handler(const cJSON *input, char *result, size_t result_len)
{
    (void)input;

    // Release any prior pending image
    media_release_pending();

    const uint8_t *jpeg_buf = NULL;
    size_t jpeg_len = 0;

    if (!camera_capture_jpeg(&jpeg_buf, &jpeg_len)) {
        snprintf(result, result_len, "Error: camera capture failed");
        return false;
    }

    // Base64-encode the JPEG for LLM vision
    size_t b64_len = 0;
    char *b64 = base64_encode(jpeg_buf, jpeg_len, &b64_len);

    if (!b64) {
        camera_release_frame();
        snprintf(result, result_len, "Error: out of memory encoding image (%u bytes)",
                 (unsigned)jpeg_len);
        return false;
    }

    s_pending_b64 = b64;
    s_pending_b64_len = b64_len;

    ESP_LOGI(TAG, "Photo captured: %u bytes JPEG, %u bytes base64",
             (unsigned)jpeg_len, (unsigned)b64_len);

    snprintf(result, result_len,
             "Photo captured successfully (%u bytes JPEG). "
             "The image is attached for your visual analysis.",
             (unsigned)jpeg_len);
    return true;
}

#endif // ZCLAW_HAS_CAMERA

// ---------------------------------------------------------------------------
// record_audio tool handler
// ---------------------------------------------------------------------------
#if ZCLAW_HAS_MICROPHONE

bool tools_record_audio_handler(const cJSON *input, char *result, size_t result_len)
{
    // Parse optional duration_ms (default 3 seconds)
    uint32_t duration_ms = MEDIA_AUDIO_DEFAULT_MS;
    cJSON *dur = cJSON_GetObjectItem(input, "duration_ms");
    if (dur && cJSON_IsNumber(dur)) {
        duration_ms = (uint32_t)dur->valuedouble;
    }

    if (duration_ms > (uint32_t)(MIC_RECORD_SECS_MAX * 1000)) {
        duration_ms = MIC_RECORD_SECS_MAX * 1000;
    }
    if (duration_ms < 100) {
        duration_ms = 100;
    }

    int16_t *audio_buf = NULL;
    size_t audio_len = 0;

    if (!mic_record(duration_ms, &audio_buf, &audio_len)) {
        snprintf(result, result_len, "Error: audio recording failed");
        return false;
    }

    // Base64-encode the PCM data
    size_t b64_len = 0;
    char *b64 = base64_encode((const uint8_t *)audio_buf, audio_len, &b64_len);
    free(audio_buf);

    if (!b64) {
        snprintf(result, result_len,
                 "Error: out of memory encoding audio (%u bytes)", (unsigned)audio_len);
        return false;
    }

    ESP_LOGI(TAG, "Audio recorded: %u ms, %u bytes PCM, %u bytes base64",
             (unsigned)duration_ms, (unsigned)audio_len, (unsigned)b64_len);

    snprintf(result, result_len,
             "Audio recorded: %u ms, %u bytes (16kHz 16-bit mono PCM, base64-encoded). "
             "data:audio/pcm;base64,%.64s%s",
             (unsigned)duration_ms, (unsigned)audio_len,
             b64, b64_len > 64 ? "..." : "");

    free(b64);
    return true;
}

#endif // ZCLAW_HAS_MICROPHONE

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------
#ifdef TEST_BUILD

void media_test_inject_image(const char *b64_data, size_t b64_len)
{
    media_release_pending();
    if (b64_data && b64_len > 0) {
        s_pending_b64 = malloc(b64_len + 1);
        if (s_pending_b64) {
            memcpy(s_pending_b64, b64_data, b64_len);
            s_pending_b64[b64_len] = '\0';
            s_pending_b64_len = b64_len;
        }
    }
}

// Expose base64_encode for unit testing
char *media_test_base64_encode(const uint8_t *data, size_t data_len, size_t *out_len)
{
    return base64_encode(data, data_len, out_len);
}

#endif // TEST_BUILD
