#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "esp_sleep.h"
#include "esp_camera.h"
#include <dirent.h>

// MicroSD
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

esp_err_t initAiThinkerCamera();
esp_err_t initSdcard();
void savePhoto();
int getNumberOfFiles();

int fileNumber = 0;
unsigned long previousMillis = 0;   // last time image was sent
int64_t wakeTime = 20000000;

void app_main(void)
{
    // Camera init
    esp_err_t status = initAiThinkerCamera();
    if (status != ESP_OK) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        esp_deep_sleep_start();
    };

    // SD Card init
    status = initSdcard();
    if (status != ESP_OK) {
        printf("SD Card init failed with error 0x%x\n", status);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        esp_deep_sleep_start();
    }

    fileNumber = getNumberOfFiles() + 1;
    savePhoto();

    while (esp_timer_get_time() < 9000000UL) {
        if (esp_timer_get_time() - previousMillis >= 500000UL) {
          previousMillis = esp_timer_get_time();
          savePhoto();
        }
    }

    printf("sleep forever\n");
    vTaskDelay(100 / portTICK_PERIOD_MS);
    esp_deep_sleep_start();
}

esp_err_t initSdcard()
{
  esp_err_t ret = ESP_FAIL;
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 1,
  };
  sdmmc_card_t *card;

  printf("Mounting SD card...\n");
  ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

  if (ret == ESP_OK) {
    printf("SD card mount successfully!\n");
  }  else  {
    printf("Failed to mount SD card VFAT filesystem. Error: %s\n", esp_err_to_name(ret));
  }

  return ret;
}

int getNumberOfFiles() {
    int fileCount = 0;
    DIR * dirp;
    struct dirent * entry;

    dirp = opendir("/sdcard");
    while ((entry = readdir(dirp)) != NULL) {
        if (entry->d_type == DT_REG) {
            fileCount++;
        }
    }

    closedir(dirp);
    return fileCount;
}

void savePhoto()
{
  printf("Taking picture: ");
  camera_fb_t *fb = esp_camera_fb_get();

  char *filename = (char*)malloc(13 + sizeof(fileNumber));
  sprintf(filename, "/sdcard/%d.jpg", fileNumber);

  printf(filename);
  printf("\n");

  FILE *file = fopen(filename, "w");

  if (file != NULL)  {
    fwrite(fb->buf, 1, fb->len, file);
  }  else  {
    printf("Could not open file\n");
  }

  fclose(file);
  esp_camera_fb_return(fb);
  free(filename);
  fileNumber++;
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
  config.frame_size = FRAMESIZE_XGA; // 1024 x 768
  config.jpeg_quality = 10;
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
      printf("Init camera error\n");
      return err;
  }

  // rotate 180
  sensor_t * s = esp_camera_sensor_get();
  s->set_hmirror(s, 1); 
  s->set_vflip(s, 1);   

  return ESP_OK;
}
