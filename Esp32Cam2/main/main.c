#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_camera.h"
#include <string.h>
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_sleep.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#define WIFI_SSID      "007"
#define WIFI_PASS      ""
#define EXAMPLE_ESP_MAXIMUM_RETRY  50

//#define IPADDR_MY         ((u32_t)0x2301A8C0UL) // 192.168.1.35
#define IPADDR_MY         ((u32_t)0x0B01A8C0UL) // 192.168.1.11
#define IPADDR_GW         ((u32_t)0xFE01A8C0UL) // 192.168.1.254
#define IPADDR_NETMASK    ((u32_t)0x00FFFFFFUL) // 255.255.255.0

const char *postUrl = "http://192.168.1.19/test/a.php";

// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;
unsigned long previousMillis = 0;   // last time image was sent
unsigned int sendNumber = 1;

#define LED_BLUE 13
#define LED_RED 12
#define BUTTON1 14

esp_err_t initAiThinkerCamera();
void initWifi(void);
void wifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
esp_err_t httpEventHandler(esp_http_client_event_t *evt);
void sendPhoto();

void app_main(void)
{
    gpio_pad_select_gpio(BUTTON1);
    gpio_pad_select_gpio(LED_BLUE);
    gpio_pad_select_gpio(LED_RED);
    gpio_set_direction(BUTTON1, GPIO_MODE_INPUT);
    gpio_set_direction(LED_BLUE, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_RED, GPIO_MODE_OUTPUT);

    gpio_set_level(LED_RED, 1);
    printf("init\n");
    
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    gpio_set_level(LED_RED, 0);

    initWifi();
    initAiThinkerCamera();
    printf("started\n");

    while(1) {
        if (esp_timer_get_time() - previousMillis >= 1000000UL) {
          previousMillis = esp_timer_get_time();
          gpio_set_level(LED_BLUE, 1);
          sendPhoto();
          gpio_set_level(LED_BLUE, 0);
          vTaskDelay(50 / portTICK_PERIOD_MS);
        }

        //vTaskDelay(1000 / portTICK_PERIOD_MS);

        //esp_sleep_enable_ext0_wakeup(BUTTON1, 1);
        //esp_deep_sleep_start();
    }
}

void sendPhoto() {
  camera_fb_t *fb = esp_camera_fb_get();

  if (!fb) {
    printf("Camera capture failed\n");
  }

  esp_http_client_handle_t http_client;
  esp_http_client_config_t config_client = {0};
  config_client.url = postUrl;
  config_client.event_handler = httpEventHandler;
  config_client.method = HTTP_METHOD_POST;

  http_client = esp_http_client_init(&config_client);
  esp_http_client_set_post_field(http_client, (const char *)fb->buf, fb->len);
  esp_http_client_set_header(http_client, "Content-Type", "image/jpg");
  esp_err_t err = esp_http_client_perform(http_client);

  if (err == ESP_OK) {
    if (sendNumber < 4 || sendNumber % 10 == 0) {
      printf("send %d\n", sendNumber);
    }
  } else {
    printf("send error: %d\n", err);
  }

  sendNumber++;
  esp_http_client_cleanup(http_client);
  esp_camera_fb_return(fb); // return the frame buffer back to be reused
}

void initWifi(void) {
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifiEventHandler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifiEventHandler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .scan_method = WIFI_FAST_SCAN,
            .channel = 9
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    tcpip_adapter_ip_info_t info;
    info.ip.addr = IPADDR_MY;
    info.gw.addr = IPADDR_GW;
    info.netmask.addr = IPADDR_NETMASK;

    esp_err_t err = tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
    if(err != ESP_OK && err != ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STOPPED){
        printf("DHCP could not be stopped! Error: %d", err);
    }

    err = tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &info);
    if(err != ERR_OK){
        printf("STA IP could not be configured! Error: %d", err);
    }

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        //printf("connected to %s\n", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        printf("Failed to connect to %s\n", WIFI_SSID);
    } else {
        printf("Wifi: unknown error\n");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

void wifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        printf("connect fail\n");

        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            printf("retry to connect\n");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        /*printf("sta connected\n");
        wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*) event_data;
        
        for (int i=0; i<6; i++) {
          printf("val:%hhx", event->bssid[i]);
        }*/
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        //ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        //printf("got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t httpEventHandler(esp_http_client_event_t *evt) {
  switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
      printf("Http event error\n");
      break;
    case HTTP_EVENT_ON_CONNECTED:
      //printf("HTTP_EVENT_ON_CONNECTED\n");
      break;
    case HTTP_EVENT_HEADER_SENT:
      //printf("HTTP_EVENT_HEADER_SENT\n");
      break;
    case HTTP_EVENT_ON_HEADER:
      //printf("HTTP_EVENT_ON_HEADER, key=%s, value=%s\n", evt->header_key, evt->header_value);
      break;
    case HTTP_EVENT_ON_DATA:
      //printf("HTTP_EVENT_ON_DATA, len=%d\n", evt->data_len);
      break;
    case HTTP_EVENT_ON_FINISH:
      //printf("HTTP_EVENT_ON_FINISH\n");
      break;
    case HTTP_EVENT_DISCONNECTED:
      //printf("HTTP_EVENT_DISCONNECTED\n");
      break;
  }

  return ESP_OK;
}

esp_err_t initAiThinkerCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21; 
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;  
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_XGA; //FRAMESIZE_SVGA;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
      printf("Init camera error\n");
      return err;
  }

  return ESP_OK;
}