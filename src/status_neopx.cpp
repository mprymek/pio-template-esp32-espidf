#ifdef STATUS_NEOPX_DATA_PIN

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <SmartLeds.h>

#include "status.h"

#ifndef STATUS_NEOPX_TYPE
#define STATUS_NEOPX_TYPE LED_WS2812
#endif

#ifndef STATUS_NEOPX_CHANNEL
#define STATUS_NEOPX_CHANNEL 0
#endif

#define RED 0xff0000
#define GREEN 0xff00
#define BLUE 0xff
#define PINK 0xe6007e
#define YELLOW 0xffff00
#define WHITE 0xffffff

static const char *TAG = "stn";

static TaskHandle_t status_task_handle;
static void status_task(void *pvParameters);

static SmartLed leds(STATUS_NEOPX_TYPE, 1, STATUS_NEOPX_DATA_PIN,
                     STATUS_NEOPX_CHANNEL, SingleBuffer);

#define set_color(c)                                                           \
   leds[0] = Rgb { ((c)&0xFF0000) >> 16, ((c)&0xFF00) >> 8, (c)&0xFF }

esp_err_t status_init() { return ESP_OK; }

esp_err_t status_start(unsigned int task_priority) {
   if (xTaskCreate(&status_task, "sta", 2048, NULL, task_priority,
                   &status_task_handle) != pdPASS) {
      return ESP_ERR_NO_MEM;
   }

   return ESP_OK;
}

void status_set(uint32_t flags) {
   xTaskNotify(status_task_handle, flags, eSetBits);
}

void status_clear(uint32_t flags) {
   xTaskNotify(status_task_handle, flags << 16, eSetBits);
}

static void status_task(void *pvParameters) {
   uint32_t notify_val;
   uint32_t status = 0;
   for (;;) {
      if (!xTaskNotifyWait(0, 0xFFFFFFFF, &notify_val, portMAX_DELAY)) {
         continue;
      }
      // set bits
      status |= notify_val & 0xFFFF;
      // clear bits
      status &= ~(notify_val >> 16);
      ESP_LOGD(TAG, "status = %lu", status);

      if (status == 0) {
         set_color(0);
      } else if (status & STATUS_ERROR) {
         set_color(RED);
      } else if (status & STATUS_NET_DISCONNECTED) {
         set_color(PINK);
      } else if (status & STATUS_BOOTING) {
         set_color(GREEN);
      } else if (status & STATUS_UPGRADING) {
         set_color(BLUE);
      }
      leds.show();
      leds.wait();
   }
}

#endif // STATUS_NEOPX_DATA_PIN
