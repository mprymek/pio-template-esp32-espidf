#ifndef STATUS_NEOPX_DATA_PIN

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "status.h"

static const char *TAG = "stn";

static uint32_t status = 0;

static void log_status();

esp_err_t status_init() { return ESP_OK; }

esp_err_t status_start(unsigned int task_priority) { return ESP_OK; }

void status_set(uint32_t flags) {
   status |= flags;
   log_status();
}

void status_clear(uint32_t flags) {
   status &= ~flags;
   log_status();
}

static void log_status() {
   esp_log_write(ESP_LOG_INFO, TAG,
                 LOG_COLOR_I "I (%lu) status =", esp_log_timestamp());
   if (status & STATUS_ERROR) {
      esp_log_write(ESP_LOG_INFO, TAG, " ERROR");
   }
   if (status & STATUS_BOOTING) {
      esp_log_write(ESP_LOG_INFO, TAG, " BOOTING");
   }
   if (status & STATUS_NET_DISCONNECTED) {
      esp_log_write(ESP_LOG_INFO, TAG, " NET_DISCON");
   }
   esp_log_write(ESP_LOG_INFO, TAG, "\n");
}

#endif // ifndef STATUS_NEOPX_DATA_PIN
