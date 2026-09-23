#pragma once

#include "esp_err.h"
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    framesize_t frame_size;
    pixformat_t pixel_format;
    int jpeg_quality;
    int fb_count;
} camera_config_params_t;

esp_err_t camera_module_init(const camera_config_params_t *params);

esp_err_t camera_module_deinit(void);

camera_fb_t* camera_module_capture(void);

void camera_module_return_fb(camera_fb_t *fb);

esp_err_t camera_module_set_quality(int quality);

esp_err_t camera_module_set_brightness(int brightness);

esp_err_t camera_module_set_contrast(int contrast);

esp_err_t camera_module_set_saturation(int saturation);

#ifdef __cplusplus
}
#endif