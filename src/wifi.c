#if defined(WIFI_DEBUG)
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#endif

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"

#include "aux.h"
#include "secrets.h"
#include "status.h"

#ifndef WIFI_AP_CHANNEL
#define WIFI_AP_CHANNEL 1
#endif

#ifndef WIFI_AP_MAX_CONN
#define WIFI_AP_MAX_CONN 10
#endif

// The weakest authmode to accept.
#ifndef WIFI_STA_MIN_AUTHMODE
#define WIFI_STA_MIN_AUTHMODE WIFI_AUTH_WPA2_PSK
#endif

#ifndef WIFI_STA_MAX_RETRIES
#define WIFI_STA_MAX_RETRIES 5
#endif

static const char *TAG = "wif";

//
// AP mode
//

#ifdef WIFI_AP_SSID

static void ap_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data);

esp_err_t wifi_init_ap(void) {
   status_set(STATUS_NET_DISCONNECTED);

   ERR_CHECK(esp_netif_init());
   ERR_CHECK(esp_event_loop_create_default());
   esp_netif_create_default_wifi_ap();

   wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
   ERR_CHECK(esp_wifi_init(&cfg));

   ERR_CHECK(esp_event_handler_instance_register(
       WIFI_EVENT, ESP_EVENT_ANY_ID, &ap_event_handler, NULL, NULL));

   return ESP_OK;
}

esp_err_t wifi_start_ap(void) {
   wifi_config_t wifi_config = {
       .ap =
           {
               .ssid = WIFI_AP_SSID,
               .channel = WIFI_AP_CHANNEL,
               .max_connection = WIFI_AP_MAX_CONN,
#ifndef WIFI_AP_PASSWD
               .authmode = WIFI_AUTH_OPEN,
#else
               .password = WIFI_AP_PASSWD,
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
               .authmode = WIFI_AUTH_WPA3_PSK,
               .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else
               .authmode = WIFI_AUTH_WPA2_PSK,
#endif
#endif
               .pmf_cfg =
                   {
                       .required = true,
                   },
           },
   };

   ERR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
   ERR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
   ERR_CHECK(esp_wifi_start());
   ESP_LOGI(TAG, "SSID: \"%s\" channel: %d", WIFI_AP_SSID, WIFI_AP_CHANNEL);

   status_clear(STATUS_NET_DISCONNECTED);

   return ESP_OK;
}

static void ap_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data) {
   wifi_event_ap_staconnected_t *ev_connected;
   wifi_event_ap_stadisconnected_t *ev_disconnected;

   switch (event_id) {
   case WIFI_EVENT_AP_STACONNECTED:
      ev_connected = (wifi_event_ap_staconnected_t *)event_data;
      ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
               MAC2STR(ev_connected->mac), ev_connected->aid);
      break;
   case WIFI_EVENT_AP_STADISCONNECTED:
      ev_disconnected = (wifi_event_ap_stadisconnected_t *)event_data;
      ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
               MAC2STR(ev_disconnected->mac), ev_disconnected->aid);
      break;
   }
}

#endif // ifdef WIFI_AP_SSID

//
// STA mode
//

#ifdef WIFI_STA_SSID

static EventGroupHandle_t sta_evt_grp;
#define STA_CONNECTED_BIT BIT0
#define STA_CONN_FAILED_BIT BIT1

static int sta_retry_cnt = 0;

static void sta_wifi_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data);
static void sta_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data);

esp_err_t wifi_init_sta() {
   status_set(STATUS_NET_DISCONNECTED);

   sta_evt_grp = xEventGroupCreate();

   ERR_CHECK(esp_netif_init());

   ERR_CHECK(esp_event_loop_create_default());
   esp_netif_create_default_wifi_sta();

   wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
   ERR_CHECK(esp_wifi_init(&cfg));

   esp_event_handler_instance_t instance_any_id;
   esp_event_handler_instance_t instance_got_ip;
   ERR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                 &sta_wifi_event_handler, NULL,
                                                 &instance_any_id));
   ERR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                 &sta_ip_event_handler, NULL,
                                                 &instance_got_ip));

   return ESP_OK;
}

esp_err_t wifi_start_sta() {
   ESP_LOGI(TAG, "connecting to SSID \"%s\"", WIFI_STA_SSID);

   wifi_config_t wifi_config = {
       .sta =
           {
               .ssid = WIFI_STA_SSID,
#ifdef WIFI_STA_PASSWD
               .password = WIFI_STA_PASSWD,
               .threshold.authmode = WIFI_STA_MIN_AUTHMODE,
   //.sae_pwe_h2e = ESP_WPA3_SAE_PWE_BOTH,
   //.sae_h2e_identifier = WIFI_STA_H2E_IDENTIFIER,
#endif
           },
   };
   ERR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
   ERR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
   ERR_CHECK(esp_wifi_start());

   EventBits_t bits =
       xEventGroupWaitBits(sta_evt_grp, STA_CONNECTED_BIT | STA_CONN_FAILED_BIT,
                           pdFALSE, pdFALSE, portMAX_DELAY);

   if (bits & STA_CONNECTED_BIT) {
      return ESP_OK;
   }

   ESP_LOGE(TAG, "Failed to connect");

   return ESP_FAIL;
}

static void sta_wifi_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data) {
   switch (event_id) {
   case WIFI_EVENT_STA_START:
      esp_wifi_connect();
      break;
   case WIFI_EVENT_STA_DISCONNECTED:
      status_set(STATUS_NET_DISCONNECTED);
      if (sta_retry_cnt < WIFI_STA_MAX_RETRIES) {
         esp_wifi_connect();
         sta_retry_cnt++;
         ESP_LOGI(TAG, "connection failed, retrying (%d/%d)", sta_retry_cnt,
                  WIFI_STA_MAX_RETRIES);
      } else {
         ESP_LOGI(TAG, "connection failed, max retrys (%d) reached",
                  WIFI_STA_MAX_RETRIES);
         xEventGroupSetBits(sta_evt_grp, STA_CONN_FAILED_BIT);
      }
      break;
   }
}

static void sta_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data) {
   switch (event_id) {
   case IP_EVENT_STA_GOT_IP:
      status_clear(STATUS_NET_DISCONNECTED);
      sta_retry_cnt = 0;
      xEventGroupSetBits(sta_evt_grp, STA_CONNECTED_BIT);
      break;
   }
}

#endif // ifdef WIFI_STA_SSID
