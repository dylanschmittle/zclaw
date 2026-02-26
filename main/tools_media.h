#ifndef TOOLS_MEDIA_H
#define TOOLS_MEDIA_H

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Initialize media subsystems (camera/mic) based on board features.
// Called during startup. Safe to call even if no media hardware is present.
void media_init(void);

// Pending image state for vision integration.
// After capture_photo executes, the JPEG data is held here until the
// next LLM request is built, then released.

// Check if a captured image is waiting to be sent to the LLM.
bool media_has_pending_image(void);

// Get the pending base64-encoded JPEG data and associated tool_use_id.
// Returns false if no image is pending.
bool media_get_pending_image(const char **base64_out, size_t *base64_len_out,
                             const char **tool_id_out);

// Set the tool_use_id for the pending image (called by agent after tool exec).
void media_set_pending_tool_id(const char *tool_id);

// Release the pending image data and camera frame buffer.
void media_release_pending(void);

#ifdef TEST_BUILD
// Inject base64 image data for testing (bypasses camera capture).
void media_test_inject_image(const char *b64_data, size_t b64_len);

// Expose base64 encoder for testing.
char *media_test_base64_encode(const uint8_t *data, size_t data_len, size_t *out_len);
#endif

#endif // TOOLS_MEDIA_H
