#ifndef CAMERA_H
#define CAMERA_H

#include "config.h"

#if ZCLAW_HAS_CAMERA

#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

// Initialize the OV2640 camera sensor.
// Returns ESP_OK on success, error code on failure.
esp_err_t camera_init(void);

// Capture a single JPEG frame.
// On success, sets *buf to the JPEG data and *len to its size.
// The caller must call camera_release_frame() when done with the buffer.
// Returns true on success, false on failure.
bool camera_capture_jpeg(const uint8_t **buf, size_t *len);

// Release a previously captured frame buffer.
void camera_release_frame(void);

// Deinitialize the camera and free resources.
void camera_deinit(void);

#endif // ZCLAW_HAS_CAMERA
#endif // CAMERA_H
