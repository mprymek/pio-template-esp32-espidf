#if defined(OTA_DEBUG)
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#endif

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"

#include "aux.h"
#include "ota.h"
#include "status.h"

// First upgrade availability check delay [ms]
#ifndef OTA_FIRST_CHECK_DELAY
#define OTA_FIRST_CHECK_DELAY 5000
#endif

// Upgrade availability check delay [ms]
#ifndef OTA_CHECK_DELAY
#define OTA_CHECK_DELAY OTA_FIRST_CHECK_DELAY
#endif

static const char *TAG = "ota";

// Maximal ETag length including trailing zero.
#define ETAG_MAX_LEN 33
// Maximal partition label length including trailing zero.
// Adjust to esp_partition_t.label size.
#define MAX_PART_LABEL_LEN 17
// Maximal length of the NVS key for partition etag including trailing zero.
#define MAX_ETAG_KEY_LEN (sizeof("etag_") - 1 + MAX_PART_LABEL_LEN)

#define OTA_NVS_NAMESPACE "ota"

// TODO: Use these functions:
// esp_err_tesp_ota_mark_app_valid_cancel_rollback(void);
// esp_err_tesp_ota_mark_app_invalid_rollback_and_reboot(void);

typedef struct {
   char etag[ETAG_MAX_LEN];
} upgrade_http_handler_data_t;

static void ota_task(void *pvParameters);
static esp_err_t get_current_etag();
static bool is_upgrade_available(const char *firmware_url, const char *etag);
static esp_err_t upgrade(const char *firmware_url);
static esp_err_t upgrade_http_handler(esp_http_client_event_t *evt);

static TaskHandle_t ota_task_handle;
static nvs_handle_t ota_nvs_handle;
static char current_etag[ETAG_MAX_LEN];
//extern const uint8_t isrgrootx1_pem_start[] asm("_binary_isrgrootx1_pem_start");

esp_err_t ota_init() {
   esp_err_t err;

   if ((err = nvs_open(OTA_NVS_NAMESPACE, NVS_READWRITE, &ota_nvs_handle)) != ESP_OK) {
      return err;
   }

   if ((err = get_current_etag()) !=       ESP_OK) {
      ESP_LOGW(TAG, "Partition ETag read failed: %s",
               esp_err_to_name(err));
      current_etag[0] = 0;
   }

   return ESP_OK;
}

esp_err_t ota_start(unsigned int task_priority) {
   if (xTaskCreate(&ota_task, TAG, 8*1024, NULL, task_priority,
                   &ota_task_handle) != pdPASS) {
      return ESP_ERR_NO_MEM;
   }

   return ESP_OK;
}

esp_err_t ota_boot_factory() {
   esp_err_t err;
   esp_partition_iterator_t part_iter;

   if ((part_iter = esp_partition_find(ESP_PARTITION_TYPE_APP,
                                       ESP_PARTITION_SUBTYPE_APP_FACTORY,
                                       "factory")) == NULL) {
      return ESP_ERR_NOT_FOUND;
   }

   const esp_partition_t *factory_part = esp_partition_get(part_iter);
   esp_partition_iterator_release(part_iter);
   if ((err = esp_ota_set_boot_partition(factory_part)) != ESP_OK) {
      return err;
   }
   ESP_LOGI(TAG, "Going back to factory image!");
   esp_restart();

   // Never reached.
   return ESP_OK;
}

static void ota_task(void *pvParameters) {
   esp_err_t err;

   vTaskDelay(OTA_FIRST_CHECK_DELAY / portTICK_PERIOD_MS);
   for (;;) {
      if (is_upgrade_available(OTA_URL, current_etag)) {
         if ((err = upgrade(OTA_URL)) != ESP_OK) {
            ESP_LOGE(TAG, "Upgrade failed: %s", esp_err_to_name(err));
         } else {
            nvs_close(ota_nvs_handle);
            esp_restart();
         }
      }
      vTaskDelay(OTA_CHECK_DELAY / portTICK_PERIOD_MS);
   }
}

inline static void partition_etag_key(const char *part_label, char *nvs_key,
                               size_t nvs_key_len) {
   snprintf(nvs_key, nvs_key_len, "etag_%s", part_label);
}

static esp_err_t get_current_etag() {
   esp_err_t err;
   char nvs_key[MAX_ETAG_KEY_LEN];

   const esp_partition_t *curr_part = esp_ota_get_running_partition();
   ESP_LOGI(TAG, "Current partition: %s", curr_part->label);

   if (strcmp(curr_part->label, "factory") == 0) {
      current_etag[0] = 0;
      return ESP_OK;
   }

   // Read partition ETag from NVS.
   size_t size;
   partition_etag_key(curr_part->label, nvs_key, sizeof(nvs_key));
   if ((err = nvs_get_str(ota_nvs_handle, nvs_key, NULL, &size)) != ESP_OK) {
      return err;
   }
   if (size > sizeof(current_etag)) {
      // Should not happen.
      current_etag[0] = 0;
      return ESP_OK;
   }
   if ((err = nvs_get_str(ota_nvs_handle, nvs_key, current_etag, &size)) != ESP_OK) {
      return err;
   }
   ESP_LOGI(TAG, "Current ETag: %s", current_etag);

   return ESP_OK;
}

static esp_err_t http_head(const char *url, int *status_code,
                           const char *etag) {
   esp_err_t err;

   esp_http_client_config_t config = {
       .url = url,
       .method = HTTP_METHOD_HEAD,
       .event_handler = NULL,
   };

   esp_http_client_handle_t client = esp_http_client_init(&config);
   char etag2[ETAG_MAX_LEN + 3];
   snprintf(etag2, sizeof(etag2), "\"%s\"", etag);
   esp_http_client_set_header(client, "If-None-Match", etag2);
   if ((err = esp_http_client_perform(client)) == ESP_OK) {
      *status_code = esp_http_client_get_status_code(client);
   }
   esp_http_client_cleanup(client);

   return err;
}

// Determine if a new firmware is available.
static bool is_upgrade_available(const char *firmware_url, const char *etag) {
   esp_err_t err;
   int status_code;

   if ((err = http_head(firmware_url, &status_code,
                        etag)) != ESP_OK) {
      return false;
   }
   if (status_code == 200) {
      ESP_LOGI(TAG, "Update available");
      return true;
   }
   if (status_code == 304 /* NOT MODIFIED */) {
      ESP_LOGD(TAG, "No update available");
      return false;
   }
   ESP_LOGW(TAG, "HEAD status = %d", status_code);
   return false;
}

static esp_err_t upgrade(const char *firmware_url) {
   esp_err_t err;

   status_set(STATUS_UPGRADING);

   upgrade_http_handler_data_t handler_data = {
       .etag = {0},
   };

   esp_http_client_config_t http_config = {
       .url = firmware_url,
       //.cert_pem = (char *)isrgrootx1_pem_start,
       .event_handler = upgrade_http_handler,
       .user_data = &handler_data,
       .keep_alive_enable = true,
   };

   esp_https_ota_config_t ota_config = {
      .http_config = &http_config,
      //.partial_http_download = false,
   };

   const esp_partition_t *next_part = esp_ota_get_next_update_partition(NULL);
   ESP_LOGI(TAG, "Next partition: %s", next_part->label);

   ESP_LOGI(TAG, "Attempting to download update from %s", http_config.url);
   if ((err = esp_https_ota(&ota_config)) != ESP_OK) {
      goto END;
   }

   // Write ETag to NVS.
   char nvs_key[MAX_ETAG_KEY_LEN];
   partition_etag_key(next_part->label, nvs_key, sizeof(nvs_key));
   if ((err = nvs_set_str(ota_nvs_handle, nvs_key, handler_data.etag)) !=
       ESP_OK) {
      ESP_LOGE(TAG, "ETag write failed: %s", esp_err_to_name(err));
      goto END;
   }
   if ((err = nvs_commit(ota_nvs_handle)) != ESP_OK) {
      ESP_LOGE(TAG, "NVS commit failed: %s", esp_err_to_name(err));
      goto END;
   }

   ESP_LOGI(TAG, "Firmware upgrade OK, ETag=%s", handler_data.etag);
   err = ESP_OK;

END:
   status_clear(STATUS_UPGRADING);
   return err;
}

static esp_err_t upgrade_http_handler(esp_http_client_event_t *evt) {
   upgrade_http_handler_data_t *data = evt->user_data;

   switch (evt->event_id) {
   case HTTP_EVENT_ON_HEADER:
      ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key,
               evt->header_value);
      if (!strcmp(evt->header_key, "ETag")) {
         // Copy ETag without parentheses.
         size_t etag_len = MIN(strlen(evt->header_value) - 2, ETAG_MAX_LEN - 1);
         strncpy(data->etag, evt->header_value + 1, etag_len);
         data->etag[etag_len] = 0;
      }
      break;
   default:
      break;
   }
   return ESP_OK;
}
