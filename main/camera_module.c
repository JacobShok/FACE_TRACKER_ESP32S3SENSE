#include "camera_module.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "camera_module";

#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    10
#define CAM_PIN_SIOD    40
#define CAM_PIN_SIOC    39

#define CAM_PIN_D7      48
#define CAM_PIN_D6      11
#define CAM_PIN_D5      12
#define CAM_PIN_D4      14
#define CAM_PIN_D3      16
#define CAM_PIN_D2      18
#define CAM_PIN_D1      17
#define CAM_PIN_D0      15
#define CAM_PIN_VSYNC   38
#define CAM_PIN_HREF    47
#define CAM_PIN_PCLK    13

static bool camera_initialized = false;

esp_err_t camera_module_init(const camera_config_params_t *params)
{
    if (camera_initialized) {
        ESP_LOGW(TAG, "Camera already initialized");
        return ESP_OK;
    }

    camera_config_t config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,   // TEST: let the native S3 clock generator drive XCLK again
                                    // (was forced to -1 for the OV2640's slow-clock-startup issue;
                                    // testing whether the OV3660 actually needs that workaround)
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,

        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,

        .xclk_freq_hz = 10000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = params->pixel_format,
        .frame_size = params->frame_size,

        .jpeg_quality = params->jpeg_quality,
        .fb_count = params->fb_count,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    };

    if (config.pixel_format == PIXFORMAT_JPEG) {
        config.jpeg_quality = params->jpeg_quality;
        if (params->frame_size > FRAMESIZE_SVGA) {
            config.fb_count = 1;
        }
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed with error 0x%x", err);
        return err;
    }

    camera_initialized = true;
    ESP_LOGI(TAG, "Camera initialized successfully");
    return ESP_OK;
}

esp_err_t camera_module_deinit(void)
{
    if (!camera_initialized) {
        ESP_LOGW(TAG, "Camera not initialized");
        return ESP_OK;
    }

    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera deinit failed with error 0x%x", err);
        return err;
    }

    camera_initialized = false;
    ESP_LOGI(TAG, "Camera deinitialized");
    return ESP_OK;
}

camera_fb_t* camera_module_capture(void)
{
    if (!camera_initialized) {
        ESP_LOGE(TAG, "Camera not initialized");
        return NULL;
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Camera capture failed");
        return NULL;
    }

    ESP_LOGI(TAG, "Picture taken! Size: %zu bytes", fb->len);
    return fb;
}

void camera_module_return_fb(camera_fb_t *fb)
{
    if (fb) {
        esp_camera_fb_return(fb);
    }
}

esp_err_t camera_module_set_quality(int quality)
{
    if (!camera_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    sensor_t *sensor = esp_camera_sensor_get();
    if (!sensor) {
        return ESP_FAIL;
    }

    if (sensor->set_quality(sensor, quality) != 0) {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "JPEG quality set to %d", quality);
    return ESP_OK;
}

esp_err_t camera_module_set_brightness(int brightness)
{
    if (!camera_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    sensor_t *sensor = esp_camera_sensor_get();
    if (!sensor) {
        return ESP_FAIL;
    }

    if (sensor->set_brightness(sensor, brightness) != 0) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t camera_module_set_contrast(int contrast)
{
    if (!camera_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    sensor_t *sensor = esp_camera_sensor_get();
    if (!sensor) {
        return ESP_FAIL;
    }

    if (sensor->set_contrast(sensor, contrast) != 0) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t camera_module_set_saturation(int saturation)
{
    if (!camera_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    sensor_t *sensor = esp_camera_sensor_get();
    if (!sensor) {
        return ESP_FAIL;
    }

    if (sensor->set_saturation(sensor, saturation) != 0) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t camera_module_set_whitebal(int enable)
{
    if (!camera_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    sensor_t *sensor = esp_camera_sensor_get();
    if (!sensor) {
        return ESP_FAIL;
    }

    if (sensor->set_whitebal(sensor, enable) != 0) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t camera_module_set_wb_mode(int mode)
{
    if (!camera_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    sensor_t *sensor = esp_camera_sensor_get();
    if (!sensor) {
        return ESP_FAIL;
    }

    if (sensor->set_wb_mode(sensor, mode) != 0) {
        return ESP_FAIL;
    }

    return ESP_OK;
}