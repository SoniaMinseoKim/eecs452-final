/* BSD Socket API Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "esp_camera.h"

#ifdef CONFIG_EXAMPLE_SOCKET_IP_INPUT_STDIN
#include "addr_from_stdin.h"
#endif

#if defined(CONFIG_EXAMPLE_IPV4)
#define HOST_IP_ADDR CONFIG_EXAMPLE_IPV4_ADDR
#elif defined(CONFIG_EXAMPLE_IPV6)
#define HOST_IP_ADDR CONFIG_EXAMPLE_IPV6_ADDR
#else
#define HOST_IP_ADDR ""
#endif

#define CAM_ID 2
#define PORT CONFIG_EXAMPLE_PORT

#define BOARD_WROVER_KIT

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


static const char *TAG = "example";
static char *payload = "Message from ESP32 ";

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
    .frame_size = FRAMESIZE_XGA,    //QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has 
    .fb_location = CAMERA_FB_IN_PSRAM,

    .jpeg_quality = 63, //0-63, for OV series camera sensors, lower number means higher quality
    .fb_count = 1,       //When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};
void send_image_over_serial(camera_fb_t *pic) {
    for (size_t i = 0; i < pic->len; i++) {
        printf("%02X", pic->buf[i]);
    }  
    printf("%%%%");
}
static void udp_client_task(void *pvParameters)
{
    char rx_buffer[128];
    char host_ip[] = HOST_IP_ADDR;
    int addr_family = 0;
    int ip_protocol = 0;
#if defined(CONFIG_EXAMPLE_IPV4)
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
#elif defined(CONFIG_EXAMPLE_IPV6)
        struct sockaddr_in6 dest_addr = { 0 };
        inet6_aton(HOST_IP_ADDR, &dest_addr.sin6_addr);
        dest_addr.sin6_family = AF_INET6;
        dest_addr.sin6_port = htons(PORT);
        dest_addr.sin6_scope_id = esp_netif_get_netif_impl_index(EXAMPLE_INTERFACE);
        addr_family = AF_INET6;
        ip_protocol = IPPROTO_IPV6;
#elif defined(CONFIG_EXAMPLE_SOCKET_IP_INPUT_STDIN)
        struct sockaddr_storage dest_addr = { 0 };
        ESP_ERROR_CHECK(get_addr_from_stdin(PORT, SOCK_DGRAM, &ip_protocol, &addr_family, &dest_addr));
#endif
    while(1){}     
}

static esp_err_t init_camera(void)
{
    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        return err;
    }

    return ESP_OK;
}
#endif
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
   
    #if ESP_CAMERA_SUPPORTED
    if(ESP_OK != init_camera()) {
        return;
    }
    sensor_t* s = esp_camera_sensor_get();
    s->set_aec_value(s, 0);
    s->set_ae_level(s, -2);
    s->set_aec2(s, 0);
    s->set_brightness(s, -2); 

    char rx_buffer[128];
    char host_ip[] = HOST_IP_ADDR;
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;
    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        }

        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        ESP_LOGI(TAG, "Socket created, sending to %s:%d", HOST_IP_ADDR, PORT);

    while (1)
    {
        camera_fb_t *pic = esp_camera_fb_get();
        pic->buf[pic->len] = CAM_ID; 
        int err = sendto(sock, pic->buf, pic->len + 1, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        }
        ESP_LOGI(TAG, "Message sent");
        
        esp_camera_fb_return(pic);
    }
    
#else
    ESP_LOGE(TAG, "Camera support is not available for this chip");
    return;
#endif

}
