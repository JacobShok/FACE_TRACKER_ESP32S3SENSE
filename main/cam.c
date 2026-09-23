#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "camera_module.h"
#include "face_detect.h"
#include "esp_timer.h"
#include "freertos/semphr.h"
#include "driver/ledc.h"

static const char *TAG = "simple_camera";

// XIAO ESP32S3 Sense camera XCLK pin (must match CAM_PIN_XCLK in camera_module.c)
#define CAM_XCLK_IO       10
#define CAM_XCLK_FREQ_HZ  20000000

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
    app_data_t *app_data = (app_data_t *)pvParameters;

    while(1) {
        xSemaphoreTake(app_data->camera_capture_sem, portMAX_DELAY);
        
        if (app_data->fb) {
            ESP_LOGI(TAG, "Running inference on captured frame...");
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

    s_start_xclk();
    vTaskDelay(pdMS_TO_TICKS(10));

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
    
    camera_module_set_brightness(0);
    camera_module_set_contrast(0);
    camera_module_set_saturation(0);
    
    ESP_LOGI(TAG, "Starting capture task...");
    xTaskCreate(capture_task, "capture_task", 4096, (void *)app_data, 3, NULL);
    ESP_LOGI(TAG, "Starting inference task...");
    xTaskCreate(inference_task, "inference_task", 8192, (void *)app_data, 1, NULL);
    ESP_LOGI(TAG, "Starting servo task...");
    xTaskCreate(servo_task, "servo_task", 2048, NULL, 2, NULL);

    ESP_LOGI(TAG, "Camera system ready! Taking photos every %d seconds", CAPTURE_INTERVAL_MS / 1000);
}