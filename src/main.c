#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "rom/ets_sys.h"

#include "aux.h"
#include "secrets.h"
#include "status.h"
#include "wifi.h"

static const char *APP_NAME = "Esp32 Template App";
static const char *APP_VERSION = "0.0.1";

static const char *TAG = "mai";
#define LINE "--------------------------------------------------\n"

static esp_err_t boot();
static esp_err_t nvs_init();

void app_main() {
   if (boot() != ESP_OK) {
      esp_log_write(ESP_LOG_ERROR, TAG,
                    LOG_COLOR_E LINE "RESTART!\n" LINE LOG_RESET_COLOR "\n");
      status_set(STATUS_ERROR);
      ets_delay_us(3000000);
      esp_restart();
   }
}

static esp_err_t boot() {
   esp_log_write(ESP_LOG_INFO, TAG, "\n\n" LINE);
   esp_log_write(ESP_LOG_INFO, TAG, "%s v.%s\n", APP_NAME, APP_VERSION);
   esp_log_write(ESP_LOG_INFO, TAG, LINE);

   BOOT_LOG("init: status", status_init());
   BOOT_LOG("start: status", status_start(tskIDLE_PRIORITY + 5));
   status_set(STATUS_BOOTING);

   // Init modules.
#if defined(WIFI_AP_SSID) || defined(WIFI_STA_SSID)
   BOOT_LOG("init: NVS flash", nvs_init());
#endif
#if defined(WIFI_AP_SSID)
   BOOT_LOG_BLOCK("init: wifi AP", wifi_init_ap());
#elif defined(WIFI_STA_SSID)
   BOOT_LOG_BLOCK("init: wifi STA", wifi_init_sta());
#endif

   // Start modules.
#if defined(WIFI_AP_SSID)
   BOOT_LOG_BLOCK("start: wifi AP", wifi_start_ap());
#elif defined(WIFI_STA_SSID)
   BOOT_LOG_BLOCK("start: wifi STA", wifi_start_sta());
#endif

   esp_log_write(ESP_LOG_INFO, TAG, LINE);
   status_clear(STATUS_BOOTING);

   return ESP_OK;
}

static esp_err_t nvs_init() {
   esp_err_t err;

   err = nvs_flash_init();
   if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_LOGI(TAG, "NVS: No free pages or new version, erasing...");
      if ((err = nvs_flash_erase()) != ESP_OK) {
         return err;
      }
      err = nvs_flash_init();
   }

   return err;
}
