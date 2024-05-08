#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t wifi_init_ap();
esp_err_t wifi_start_ap();
esp_err_t wifi_init_sta();
esp_err_t wifi_start_sta();

#ifdef __cplusplus
}
#endif
