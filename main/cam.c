#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "camera_module.h"
#include "face_detect.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/semphr.h"
#include "driver/ledc.h"
#include "img_converters.h"
#include "mbedtls/base64.h"

static const char *TAG = "simple_camera";

// TEMPORARY DEBUG: dumps one captured frame as a base64-encoded BMP over serial,
// wrapped in IMG_BEGIN/IMG_END markers, so it can be decoded and saved on the PC
// side (see tools/save_serial_frame.py). Remove once done inspecting captures.
static void s_dump_frame_over_serial(camera_fb_t *fb)
{
    uint8_t *bmp_buf = NULL;
    size_t bmp_len = 0;
    if (!frame2bmp(fb, &bmp_buf, &bmp_len)) {
        ESP_LOGE(TAG, "frame2bmp failed");
        return;
    }

    size_t b64_len = 0;
    mbedtls_base64_encode(NULL, 0, &b64_len, bmp_buf, bmp_len);
    unsigned char *b64_buf = (unsigned char *)malloc(b64_len);
    if (!b64_buf) {
        ESP_LOGE(TAG, "base64 buffer alloc failed");
        free(bmp_buf);
        return;
    }

    size_t actual_len = 0;
    mbedtls_base64_encode(b64_buf, b64_len, &actual_len, bmp_buf, bmp_len);

    printf("IMG_BEGIN\n");
    const size_t CHUNK = 256;
    for (size_t i = 0; i < actual_len; i += CHUNK) {
        size_t n = (actual_len - i < CHUNK) ? (actual_len - i) : CHUNK;
        fwrite(b64_buf + i, 1, n, stdout);
        printf("\n");
    }
    printf("IMG_END\n");
    fflush(stdout);

    free(b64_buf);
    free(bmp_buf);
}

// TEMPORARY: captures a batch of frames (1/sec) and dumps each one over serial,
// for building a real training set from this camera's actual output. Run once at
// startup, blocking, before normal capture/inference tasks begin.
static void s_capture_training_set(int count)
{
    ESP_LOGI(TAG, "Capturing training set: %d photos, 1/sec...", count);
    for (int i = 0; i < count; i++) {
        camera_fb_t *fb = camera_module_capture();
        if (!fb) {
            ESP_LOGE(TAG, "Training capture %d/%d failed", i + 1, count);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(TAG, "Training capture %d/%d", i + 1, count);
        s_dump_frame_over_serial(fb);
        camera_module_return_fb(fb);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "Training set capture complete");
}

// XIAO ESP32S3 Sense camera XCLK pin (must match CAM_PIN_XCLK in camera_module.c)
#define CAM_XCLK_IO       10
#define CAM_XCLK_FREQ_HZ  10000000

// esp32-camera's native XCLK generation on ESP32-S3 (via the LCD_CAM peripheral)
// isn't reliably running yet by the time it probes the sensor over SCCB. Pre-warm
// XCLK ourselves via LEDC first, same approach proven to work by the I2C scanner.
static void s_start_xclk(void)
{
    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_1_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = CAM_XCLK_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    ledc_channel_config_t channel_config = {
        .gpio_num = CAM_XCLK_IO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 1,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
}

typedef struct
{
    SemaphoreHandle_t camera_capture_sem;
    SemaphoreHandle_t camera_capture_sem2;
    camera_fb_t *fb;
} app_data_t;

#define CAPTURE_INTERVAL_MS  5000
#define MAX_FILES           5

static void capture_task(void *pvParameters)
{
    app_data_t *app_data = (app_data_t *)pvParameters;

    while (1) {
        ESP_LOGI(TAG, "Taking picture...");

        app_data->fb = camera_module_capture();
        if (!app_data->fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            vTaskDelay(pdMS_TO_TICKS(CAPTURE_INTERVAL_MS));
            continue;
        }

        xSemaphoreGive(app_data->camera_capture_sem);
        xSemaphoreTake(app_data->camera_capture_sem2, portMAX_DELAY);
        
        camera_module_return_fb(app_data->fb);
        
        vTaskDelay(pdMS_TO_TICKS(CAPTURE_INTERVAL_MS));
    }
}

void inference_task(void *pvParameters)
{
    esp_task_wdt_delete(NULL);
    app_data_t *app_data = (app_data_t *)pvParameters;

    while(1) {
        xSemaphoreTake(app_data->camera_capture_sem, portMAX_DELAY);
        
        if (app_data->fb) {
            ESP_LOGI(TAG, "Running inference on captured frame...");
            uint32_t sample_sum = 0;
            for (size_t i = 0; i < app_data->fb->len; i += 100) sample_sum += app_data->fb->buf[i];
            printf("Frame sample avg (raw RGB565): %lu\n", sample_sum / (app_data->fb->len / 100));
            run_face_detection((uint16_t *)app_data->fb->buf, 96*96);
            xSemaphoreGive(app_data->camera_capture_sem2);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void servo_task(void *pvParameters)
{
    while(1) {
        // Placeholder for servo control logic based on inference results
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void app_main(void)
{
    app_data_t *app_data = heap_caps_malloc(sizeof(app_data_t), MALLOC_CAP_8BIT);
    if (!app_data) {
        ESP_LOGE(TAG, "Failed to allocate app data");
        return;
    }

    app_data->camera_capture_sem = xSemaphoreCreateBinary();
    if (!app_data->camera_capture_sem) {
        ESP_LOGE(TAG, "Failed to create camera capture semaphore");
        heap_caps_free(app_data);
        return;
    }

    app_data->camera_capture_sem2 = xSemaphoreCreateBinary();
    if (!app_data->camera_capture_sem2) {
        ESP_LOGE(TAG, "Failed to create camera capture semaphore 2");
        heap_caps_free(app_data);
        return;
    }

    app_data->fb = NULL;

    ESP_LOGI(TAG, "Simple AI Camera Tracking Application Starting...");

    // TEST: disabled to let the native S3 clock generator drive XCLK instead (see camera_module.c)
    // s_start_xclk();
    // vTaskDelay(pdMS_TO_TICKS(10));

    camera_config_params_t camera_params = {
        .frame_size = FRAMESIZE_96X96,
        .pixel_format = PIXFORMAT_RGB565,
        .jpeg_quality = 10,
        .fb_count = 2
    };
    
    ESP_LOGI(TAG, "Initializing camera...");
    esp_err_t ret = camera_module_init(&camera_params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(5000));  // Wait for camera to stabilize
    
    camera_module_set_brightness(0);
    camera_module_set_contrast(0);
    camera_module_set_saturation(0);
    camera_module_set_whitebal(1);
    camera_module_set_wb_mode(0);  // Auto — a fixed preset (e.g. 3) locks manual gains and blocks real AWB convergence

    ESP_LOGI(TAG, "Starting capture task...");
    xTaskCreatePinnedToCore(capture_task, "capture_task", 4096, (void *)app_data, 3, NULL, 0);
    ESP_LOGI(TAG, "Starting inference task...");
    xTaskCreatePinnedToCore(inference_task, "inference_task", 8192, (void *)app_data, 1, NULL, 1);
    ESP_LOGI(TAG, "Starting servo task...");
    xTaskCreatePinnedToCore(servo_task, "servo_task", 2048, NULL, 2, NULL, 0);

    ESP_LOGI(TAG, "Camera system ready! Taking photos every %d seconds", CAPTURE_INTERVAL_MS / 1000);
}