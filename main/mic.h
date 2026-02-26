#ifndef MIC_H
#define MIC_H

#include "config.h"

#if ZCLAW_HAS_MICROPHONE

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Initialize the I2S PDM microphone.
// Returns ESP_OK on success, error code on failure.
esp_err_t mic_init(void);

// Record audio for up to duration_ms milliseconds.
// Allocates a buffer and sets *buf to the raw PCM data, *len to byte count.
// Caller must free *buf with free() when done.
// Returns true on success, false on failure.
bool mic_record(uint32_t duration_ms, int16_t **buf, size_t *len);

// Deinitialize the microphone and free resources.
void mic_deinit(void);

#endif // ZCLAW_HAS_MICROPHONE
#endif // MIC_H
