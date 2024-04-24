/* ESP32 decimation and blob detection algorithm
 * Authored Andrew Schallwig [arschall@umich.edu]
 * EECS 452 - Winter 2024
 * 1 April 2024 - University of Michigan, Ann Arbor
*/ 

//---------------------------- include ----------------------------
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <esp_log.h>
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_clk_tree.h"
#include "esp_system.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

//---------------------------- define ----------------------------
#define DEC_RATE 4
#define VALID_WIDTH 6 // arbitrary, need to tune
#define CAM_ID 0
#define MAX_WINDOW_SIZE 50000

#define HOST_IP_ADDR CONFIG_EXAMPLE_IPV4_ADDR
#define PORT CONFIG_EXAMPLE_PORT

#define CAM_PIN_PWDN -1  //power down is not used
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 21
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 19
#define CAM_PIN_D2 18
#define CAM_PIN_D1 5
#define CAM_PIN_D0 4
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

//---------------------------- types ----------------------------
typedef struct TagPair{
    __uint16_t wwidth;
    __uint16_t px;
    __uint16_t py;
} TagPair;

typedef struct RTPHeader {
    __uint8_t version : 2;
    __uint8_t P : 1;
    __uint8_t X : 1;
    __uint8_t CC : 4;
    __uint8_t M : 1;
    __uint8_t PT : 7;
    __uint16_t SN : 16;
    __uint32_t TS : 32;
    __uint32_t SSRC: 32;
} RTPHeader;

typedef struct TagHeader {
    __uint8_t cid : 8;
    __uint16_t px : 16;
    __uint16_t py : 16;
    suseconds_t us : 32;
    time_t s : 64;
} TagHeader;

//---------------------------- define statics ----------------------------
static camera_fb_t *pic;
static uint8_t wbuf[MAX_WINDOW_SIZE];
// static SemaphoreHandle_t mutex;
static __uint16_t task_ct;
static TaskHandle_t xHandle = NULL;
static struct sockaddr_in dest_addr;

static const char *TAG = "eecs452:delta_dec";

static char host_ip[] = HOST_IP_ADDR;
static int addr_family = AF_INET;
static int ip_protocol = IPPROTO_IP;
static int sock;

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
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

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_GRAYSCALE, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_HVGA,  // RESET: to FRAMESIZE_QVGA   //QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates.

    .jpeg_quality = 12, //0-63, for OV series camera sensors, lower number means higher quality
    .fb_count = 1,       //When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,

    .fb_location = CAMERA_FB_IN_PSRAM, // RESET: to CAMERA_FB_IN_DRAM
};

//---------------------------- methods ----------------------------
static esp_err_t init_camera(void)
{
    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }

    return ESP_OK;
}

// returns 0 if not a valid region, center px if valid TODO: is 0 ever a valid center?
bool valid_region(TagPair* tp, camera_fb_t* fb, __uint32_t *idx, __uint16_t *col) {
    // determine width of detection region
    __uint16_t px = (*idx) % fb->width; // if mod is too slow try https://lemire.me/blog/2019/02/08/faster-remainders-when-the-divisor-is-a-constant-beating-compilers-and-libdivide/
    __uint16_t py = (*idx)/fb->width;

    __uint16_t min_x = py * fb->width;
    __uint16_t max_x = (py + 1) * fb->width - 1;
    __int16_t rpx = (*idx), lpx = (*idx);

    while(rpx < max_x && fb->buf[rpx] == 255) ++rpx; // look right
    while(lpx >= min_x && fb->buf[lpx] == 255) --lpx; // look left

    __uint8_t width = rpx - lpx; // calculate width of detection 
    if(width < VALID_WIDTH) {  
        return false; // if too small to be valid, return false
    }

    // __uint16_t wwidth = width*6;
    __uint16_t wwidth = 96;
    __uint16_t whwidth = wwidth/2;

    // discard cropped apriltags
    if((whwidth > px) || (whwidth > py)) return false;
    if((px + whwidth > pic->width) || (py + whwidth > pic->height)) return false;

    tp->px = px;
    tp->py = py;
    tp->wwidth = wwidth;

    if((rpx - (*idx)) >= DEC_RATE) { // skip pixels in-width
        __uint16_t cols_moved = (rpx - (*idx)) / DEC_RATE;
        (*col) = (*col) + cols_moved;
        (*idx) = (*idx) + (DEC_RATE * cols_moved);
    }

    return true;
}

void send_image_over_serial(uint8_t *pic) {
    for (size_t i = 0; i < (9216); i++) {
        printf("%02X", pic[i]);
    }  
    printf("%%%%");
}

// send timestamp, xy, windowed bytes
void send_window(void * in) {
    TagPair* tp = (TagPair*)in;
    tp->wwidth = 96;    
    __uint16_t whwidth = tp->wwidth/2;

    ESP_LOGI(TAG, "sending at (%d, %d)", tp->px, tp->py);

    struct timeval ct = {0, 0};
    int err = gettimeofday(&ct, 0);
    if (err < 0) ESP_LOGE(TAG, "Error occurred getting system time");

    TagHeader th = {CAM_ID, tp->px, tp->py, ct.tv_sec, ct.tv_usec};
    memcpy(wbuf, &th, sizeof(th));
    __uint8_t hoffset = sizeof(TagHeader);

    __uint16_t spx = (tp->py - whwidth)*pic->width + (tp->px - whwidth);
    for(__uint16_t i = 0; i < tp->wwidth; ++i) {
        memcpy(hoffset + wbuf + (tp->wwidth*i), pic->buf + (spx + (pic->width*i)), tp->wwidth);
    }

    // send_image_over_serial(wbuf);
    int err = sendto(sock, wbuf, (tp->wwidth*tp->wwidth), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);

    --task_ct;
    free(tp);
    vTaskDelete(NULL);
}

void run_detection() {
    unsigned int start_t, end_t;
    double total_t;
    __uint32_t freq;

    while(1) {
        pic = esp_camera_fb_get();
        // send_image_over_serial(pic);

        start_t = xthal_get_ccount(); // start

        __uint16_t num_x = pic->width / DEC_RATE;
        __uint16_t col_idx = 0; 

        for(__uint32_t px = 0; px < (pic->len - 32000); px += DEC_RATE) { // stride thru cols
            if(pic->buf[px] == 255) { // delta found
                TagPair * tp = heap_caps_malloc(sizeof(TagPair), MALLOC_CAP_DEFAULT);
                tp->px = 0;
                tp->py = 0;
                tp->wwidth = 0;
                if (valid_region(tp, pic, &px, &col_idx)) { // check if this is a valid detection 
                    ESP_LOGI(TAG, "delta at (%d, %d)", tp->px, tp->py);
                    ++task_ct;
                    xTaskCreatePinnedToCore(send_window, "WSEND", CONFIG_PTHREAD_TASK_STACK_SIZE_DEFAULT, tp, tskIDLE_PRIORITY, &xHandle, 1);
                }
            }
            // stride through rows
            col_idx++;
            if(col_idx == num_x) {
                col_idx = 0;
                px += (3*pic->width);
            }
        }

        end_t = xthal_get_ccount();
        esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_EXACT, &freq);
        total_t = (double)(freq/(end_t - start_t));
        // ESP_LOGI(TAG, "at: %f fps", total_t); // freq = 160MHz

        while(task_ct > 0) {}

        esp_camera_fb_return(pic);
    }
}

//---------------------------- main ----------------------------
void app_main(void)
{
    // initalizataion
    ESP_LOGI(TAG, "initializing camera");
    if(ESP_OK != init_camera()) {
        return;
    }
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    setvbuf(stdout, NULL, _IONBF, 0);
    task_ct = 0;

    sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    sensor_t* s = esp_camera_sensor_get();
    s->set_aec_value(s, 0);
    s->set_ae_level(s, 0);
    s->set_aec2(s, 0);
    
    // start driver task
    xTaskCreatePinnedToCore(run_detection, "DCODE", CONFIG_MAIN_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, &xHandle, 0);
}