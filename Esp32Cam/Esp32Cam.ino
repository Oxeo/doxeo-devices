#include "esp_http_client.h"
#include "esp_camera.h"
#include "driver/rtc_io.h"
#include <WiFi.h>
#include <EEPROM.h>
#include "Arduino.h"

//#define MY_DEBUG;

#if defined(MY_DEBUG)
#define DEBUG_PRINT(str) Serial.println(str);
#else
#define DEBUG_PRINT(str)
#endif

const char *postUrl = "http://192.168.1.19/test/a.php";

char wifiSsid[32];
char wifiPassword[32];

const int timerInterval = 1000;    // time between each HTTP POST image
unsigned long previousMillis = 0;   // last time image was sent

enum mode_enum {NORMAL_MODE, DEBUG_MODE, REPORT_MODE, NO_WIFI};
uint8_t _mode;

#define LED_BLUE 13
#define LED_RED 12

// CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void setup() {
  Serial.begin(9600);
  
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_BLUE, LOW);
  digitalWrite(LED_RED, LOW);

  //strcpy (wifiSsid, "mySSID");
  //strcpy (wifiPassword, "myPassword");
  //writeEEPROM(0, 32, wifiSsid); //32 byte max length
  //writeEEPROM(32, 32, wifiPassword); //32 byte max length

  readEEPROM(0, 32, wifiSsid);
  readEEPROM(32, 32, wifiPassword);

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  _mode = NORMAL_MODE;
  initWifi();
  initCamera();

  if (_mode == NORMAL_MODE || _mode == REPORT_MODE) {
    sendPhoto();
    Serial.println("first image sended");
  }
}

void initWifi() {
  WiFi.mode(WIFI_STA);
  DEBUG_PRINT("Connecting to ");
  DEBUG_PRINT(wifiSsid);

  WiFi.begin(wifiSsid, wifiPassword);

  int cpt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINT(".");
    delay(500);
    cpt++;

    if (cpt > 20) {
      _mode = NO_WIFI;
      Serial.println("No wifi");
      break;
    }
  }

  if (_mode != NO_WIFI) {
    DEBUG_PRINT("ESP32-CAM IP Address: ");
    DEBUG_PRINT(WiFi.localIP());
  }
}

void initCamera() {
  DEBUG_PRINT("Init camera... ");
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  //init with high specs to pre-allocate larger buffers
  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    digitalWrite(LED_RED, HIGH);
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(5000);
    ESP.restart();
  }

  DEBUG_PRINT("Camera init done");
}

void loop() {
  while (Serial.available()) {
    String msg = Serial.readString();

    if (msg.startsWith("debug_mode")) {
      _mode = DEBUG_MODE;
    } else if (msg.startsWith("normal_mode")) {
      _mode = NORMAL_MODE;
    } else if (msg.startsWith("report_mode")) {
      _mode = REPORT_MODE;
    } else if (msg.startsWith("ssid:")) {
      String ssid = msg.substring(5);
      ssid.replace("\n", "");
      ssid.replace("\r", "");
      ssid.toCharArray(wifiSsid, sizeof(wifiSsid));
      writeEEPROM(0, 32, wifiSsid);
      Serial.print("New SSID: ");
      Serial.println(wifiSsid);
    } else if (msg.startsWith("password:")) {
      String password = msg.substring(9);
      password.replace("\n", "");
      password.replace("\r", "");
      password.toCharArray(wifiPassword, sizeof(wifiPassword));
      writeEEPROM(32, 32, wifiPassword);
      Serial.print("New password: ");
      Serial.println(wifiPassword);
    }
  }
  
  if (millis() - previousMillis >= timerInterval) {
    previousMillis = millis();

    if (_mode == NORMAL_MODE || _mode == REPORT_MODE) {
      sendPhoto();
    } else if (_mode == DEBUG_MODE) {
      Serial.println(WiFi.RSSI());
    }
  }
}

void sendPhoto() {
  DEBUG_PRINT("Taking picture...");
  digitalWrite(LED_BLUE, HIGH);
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    digitalWrite(LED_RED, HIGH);
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
  }

  DEBUG_PRINT("Sending picture...");
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
    DEBUG_PRINT("success");
    if (_mode == REPORT_MODE) {
      Serial.println("send");
    }
  } else {
    Serial.println(esp_http_client_get_status_code(http_client));
    digitalWrite(LED_RED, HIGH);
  }
  
  esp_http_client_cleanup(http_client);
  esp_camera_fb_return(fb); // return the frame buffer back to be reused
  delay(50);
  digitalWrite(LED_BLUE, LOW);
  delay(50);
}

esp_err_t httpEventHandler(esp_http_client_event_t *evt) {
  switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
      Serial.println("HTTP_EVENT_ERROR");
      break;
    case HTTP_EVENT_ON_CONNECTED:
      //Serial.println("HTTP_EVENT_ON_CONNECTED");
      break;
    case HTTP_EVENT_HEADER_SENT:
      //Serial.println("HTTP_EVENT_HEADER_SENT");
      break;
    case HTTP_EVENT_ON_HEADER:
      //Serial.println();
      //Serial.printf("HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
      break;
    case HTTP_EVENT_ON_DATA:
      //Serial.println();
      //Serial.printf("HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
      break;
    case HTTP_EVENT_ON_FINISH:
      //Serial.println("");
      //Serial.println("HTTP_EVENT_ON_FINISH");
      break;
    case HTTP_EVENT_DISCONNECTED:
      //Serial.println("HTTP_EVENT_DISCONNECTED");
      break;
  }

  return ESP_OK;
}

void writeEEPROM(int startAdr, int maxLength, char* writeString) {
  EEPROM.begin(512); //Max bytes of eeprom to use
  yield();

  for (int i = 0; i < maxLength; i++) {
    EEPROM.write(startAdr + i, writeString[i]);
    if (writeString[i] == '\0') {
      break;
    }
  }

  EEPROM.commit();
  EEPROM.end();
}

void readEEPROM(int startAdr, int maxLength, char* dest) {
  EEPROM.begin(512);
  delay(10);

  for (int i = 0; i < maxLength; i++) {
    dest[i] = char(EEPROM.read(startAdr + i));

    if (dest[i] == '\0') {
      break;
    }
  }

  EEPROM.end();
}
