#include "camera.h"

#if ZCLAW_HAS_CAMERA

#include "esp_log.h"
#include "esp_camera.h"

static const char *TAG = "camera";

static bool s_initialized = false;

esp_err_t camera_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "camera already initialized");
        return ESP_OK;
    }

    camera_config_t config = {
        .pin_pwdn     = CAM_PIN_PWDN,
        .pin_reset    = CAM_PIN_RESET,
        .pin_xclk     = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,
        .pin_d7       = CAM_PIN_D7,
        .pin_d6       = CAM_PIN_D6,
        .pin_d5       = CAM_PIN_D5,
        .pin_d4       = CAM_PIN_D4,
        .pin_d3       = CAM_PIN_D3,
        .pin_d2       = CAM_PIN_D2,
        .pin_d1       = CAM_PIN_D1,
        .pin_d0       = CAM_PIN_D0,
        .pin_vsync    = CAM_PIN_VSYNC,
        .pin_href     = CAM_PIN_HREF,
        .pin_pclk     = CAM_PIN_PCLK,

        .xclk_freq_hz = CAM_XCLK_FREQ_HZ,
        .ledc_timer   = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_JPEG,
        .frame_size   = FRAMESIZE_VGA,      // 640x480
        .jpeg_quality = CAM_JPEG_QUALITY,
        .fb_count     = CAM_FB_COUNT,
        .grab_mode    = CAMERA_GRAB_LATEST,
#if ZCLAW_HAS_PSRAM
        .fb_location  = CAMERA_FB_IN_PSRAM,
#else
        .fb_location  = CAMERA_FB_IN_DRAM,
#endif
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "camera init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "camera initialized (VGA JPEG, quality %d)", CAM_JPEG_QUALITY);
    return ESP_OK;
}

bool camera_capture_jpeg(const uint8_t **buf, size_t *len)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "camera not initialized");
        return false;
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "frame capture failed");
        return false;
    }

    *buf = fb->buf;
    *len = fb->len;
    ESP_LOGI(TAG, "captured %u bytes JPEG (%ux%u)", (unsigned)fb->len, fb->width, fb->height);
    return true;
}

void camera_release_frame(void)
{
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
        esp_camera_fb_return(fb);
    }
}

void camera_deinit(void)
{
    if (!s_initialized) {
        return;
    }
    esp_camera_deinit();
    s_initialized = false;
    ESP_LOGI(TAG, "camera deinitialized");
}

#endif // ZCLAW_HAS_CAMERA
