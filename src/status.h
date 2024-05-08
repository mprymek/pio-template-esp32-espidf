#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t status_init();
esp_err_t status_start(unsigned int task_priority);

void status_set(uint32_t flag);
void status_clear(uint32_t flag);

#define STATUS_ERROR 1
#define STATUS_BOOTING (((uint32_t)1) << 1)
#define STATUS_NET_DISCONNECTED (((uint32_t)1) << 2)

#ifdef __cplusplus
}
#endif
