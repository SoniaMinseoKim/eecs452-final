/**
 * This example takes a picture every 5s and print its size on serial monitor.
 */

// =============================== SETUP ======================================

// 1. Board setup (Uncomment):
#define BOARD_WROVER_KIT
// #define BOARD_ESP32CAM_AITHINKER

/**
 * 2. Kconfig setup
 *
 * If you have a Kconfig file, copy the content from
 *  https://github.com/espressif/esp32-camera/blob/master/Kconfig into it.
 * In case you haven't, copy and paste this Kconfig file inside the src directory.
 * This Kconfig file has definitions that allows more control over the camera and
 * how it will be initialized.
 */

/**
 * 3. Enable PSRAM on sdkconfig:
 *
 * CONFIG_ESP32_SPIRAM_SUPPORT=y
 *
 * More info on
 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html#config-esp32-spiram-support
 */

// ================================ CODE ======================================

#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_spiffs.h"
#include <stdio.h>

// support IDF 5.x
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

#include "esp_camera.h"
// #include stdio.h

#define CAM_PIN_PWDN -1  //power down is not used
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 21
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_Y9 35
#define CAM_PIN_Y8 34
#define CAM_PIN_Y7 39
#define CAM_PIN_Y6 36
#define CAM_PIN_Y5 19
#define CAM_PIN_Y4 18
#define CAM_PIN_Y3 5 
#define CAM_PIN_Y2 4 
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23 
#define CAM_PIN_PCLK 22 

// static const char *TAG = "example:take_picture";
char value; 

#if ESP_CAMERA_SUPPORTED
static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_Y9,
    .pin_d6 = CAM_PIN_Y8,
    .pin_d5 = CAM_PIN_Y7,
    .pin_d4 = CAM_PIN_Y6,
    .pin_d3 = CAM_PIN_Y5,
    .pin_d2 = CAM_PIN_Y4,
    .pin_d1 = CAM_PIN_Y3,
    .pin_d0 = CAM_PIN_Y2,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_CIF,    // XGA is largest in dram //QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has 
    .fb_location = CAMERA_FB_IN_DRAM,

    .jpeg_quality = 63, //0-63, for OV series camera sensors, lower number means higher quality
    .fb_count = 1,       //When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

// void send_image_over_serial(camera_fb_t *pic) {
//     // for (size_t i = 0; i < pic->len; i++) {
//     //     // ESP_LOG_BUFFER_HEXDUMP("tag", pic->buf, pic->len, ESP_LOG_INFO);
//     // }
//     // ESP_LOG_BUFFER_HEXDUMP("tag", pic->buf, pic->len, ESP_LOG_INFO);
//     // printf("%%%%");
//     ESP_LOG_BUFFER_HEX("tag", pic->buf, pic->len);
// }
// void send_image_over_serial(camera_fb_t *pic) {
//     for (size_t i = 0; i < pic->len; i++) {
//         // vTaskDelay(2000 / portTICK_RATE_MS);
//         value = pic->buf[i]; 
//         putchar(value);
    
//     }
// }
void send_image_over_serial(camera_fb_t *pic) {
    for (size_t i = 0; i < pic->len; i++) {
        printf("%02X", pic->buf[i]);
    }  
    printf("%%%%");
}

// void send_image_over_serial(camera_fb_t *pic) {
//     char *buffer = (char *)malloc((2 * pic->len + 5) * sizeof(char));  // Assuming 2 characters per byte and additional space for "%%%%\0"
//     size_t index = 0;
//     for (size_t i = 0; i < pic->len; i++) {
//         index += sprintf(buffer + index, "%02X", pic->buf[i]);
//     }
//     sprintf(buffer + index, "%%%%");
//     printf("%s", buffer);
//     // free(buffer);
// }
// void send_image_over_serial(camera_fb_t *pic) {
//     char sum; 
//     clock_t start_time, end_time;
//     double time_taken;
//     start_time = clock(); 
//     for (size_t i = 0; i < pic->len; i++) {
//         value = pic->buf[i];
//         sum = value + sum; 
//         // printf("%02X", pic->buf[i]);    
//     }
//     sum = sum / pic->len; 
//     end_time = clock(); 
//     time_taken = (double)(end_time - start_time) / CLOCKS_PER_SEC;
//     printf("%f \n", time_taken);
// }

// void init_spiffs() {
//     esp_vfs_spiffs_conf_t conf = {
//       .base_path = "/spiffs",
//       .partition_label = NULL,
//       .max_files = 5,
//       .format_if_mount_failed = true
//     };

//     // Use settings defined above to initialize and mount SPIFFS filesystem.
//     // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
//     esp_err_t ret = esp_vfs_spiffs_register(&conf);

//     if (ret != ESP_OK) {
//         if (ret == ESP_FAIL) {
//             ESP_LOGE(TAG, "Failed to mount or format filesystem");
//         } else if (ret == ESP_ERR_NOT_FOUND) {
//             ESP_LOGE(TAG, "Failed to find SPIFFS partition");
//         } else {
//             ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
//         }
//         return;
//     }

//     size_t total = 0, used = 0;
//     ret = esp_spiffs_info(NULL, &total, &used);
//     if (ret != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
//     } else {
//         ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
//     }
// }
// void save_image_to_filesystem(camera_fb_t *pic) {
//     // Ensure `init_spiffs();` has been called before this

//     // Use a file name like "picture.jpg"
//     const char *file_name = "/spiffs/picture.jpg";

//     // Open file for writing
//     FILE* file = fopen(file_name, "w");
//     if (file == NULL) {
//         ESP_LOGE(TAG, "Failed to open file for writing");
//         return;
//     }

//     // Write the buffer to the file
//     size_t written = fwrite(pic->buf, 1, pic->len, file);
//     if(written != pic->len) {
//         ESP_LOGE(TAG, "Failed to write complete buffer to file");
//     } else {
//         ESP_LOGI(TAG, "Image written to file %s", file_name);
//     }

//     // Close the file
//     fclose(file);
// }
static esp_err_t init_camera(void)
{
    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        // ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }

    return ESP_OK;
}
#endif

void app_main(void)
{

setvbuf(stdout, NULL, _IONBF, 0);
#if ESP_CAMERA_SUPPORTED
    if(ESP_OK != init_camera()) {
        return;
    }
sensor_t* s = esp_camera_sensor_get();
s->set_aec_value(s, 100); 
s->set_ae_level(s, 0);
s->set_aec2(s, 0);
// s->set_brightness(s, -1);
    while (1)
    {
        // ESP_LOGI(TAG, "Taking picture...");
        camera_fb_t *pic = esp_camera_fb_get();

        // use pic->buf to access the image
        // ESP_LOGI(TAG, "Picture taken! Its size was: %zu bytes", pic->len);
        send_image_over_serial(pic); 
        esp_camera_fb_return(pic);

        vTaskDelay(500 / portTICK_RATE_MS);
    }
#else
    ESP_LOGE(TAG, "Camera support is not available for this chip");
    return;
#endif
}
