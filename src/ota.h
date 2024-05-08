#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ota_init();
esp_err_t ota_start(unsigned int task_priority);
esp_err_t ota_boot_factory();

#ifdef __cplusplus
}
#endif
