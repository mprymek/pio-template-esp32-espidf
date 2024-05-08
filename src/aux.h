// Check function return value. If it's not equal to ESP_OK, return it from the
// function.
#define ERR_CHECK(x)                                                           \
   do {                                                                        \
      esp_err_t __err_rc = (x);                                                \
      if (__err_rc != ESP_OK) {                                                \
         return __err_rc;                                                      \
      }                                                                        \
   } while (0)

// Check function return value. If it's not equal to ESP_OK, print error and
// return error from the function. "TAG" must be defined.
#define ERR_CHECK_LOG(x, label)                                                \
   do {                                                                        \
      esp_err_t __err_rc = (x);                                                \
      if (__err_rc != ESP_OK) {                                                \
         ESP_LOGE(TAG, "%s: %s", (label), esp_err_to_name(__err_rc));          \
         return __err_rc;                                                      \
      }                                                                        \
   } while (0)

// Call boot function `f`. If its return value is ESP_OK, print "[OK]". If not,
// print "[ERR <return value>]" and return the value from the function.
#define BOOT_LOG(lbl, f)                                                       \
   do {                                                                        \
      esp_log_write(ESP_LOG_INFO, TAG,                                         \
                    LOG_COLOR_I "I (%lu) %s\t\t" LOG_RESET_COLOR,              \
                    esp_log_timestamp(), lbl);                                 \
      esp_err_t __err_rc = (f);                                                \
      if (__err_rc != ESP_OK) {                                                \
         esp_log_write(ESP_LOG_INFO, TAG,                                      \
                       LOG_COLOR_E "[ERR %d]\n" LOG_RESET_COLOR, __err_rc);    \
         return __err_rc;                                                      \
      }                                                                        \
      esp_log_write(ESP_LOG_INFO, TAG, LOG_COLOR_I "[OK]\n" LOG_RESET_COLOR);  \
   } while (0)

// Same as BOOT_LOG but for functions which log something (the result is not
// printed on the same line as the label).
#define BOOT_LOG_BLOCK(lbl, f)                                                 \
   do {                                                                        \
      esp_log_write(ESP_LOG_INFO, TAG,                                         \
                    LOG_COLOR_I "I (%lu) %s \t\t[..]\n" LOG_RESET_COLOR,       \
                    esp_log_timestamp(), lbl);                                 \
      esp_err_t __err_rc = (f);                                                \
      if (__err_rc != ESP_OK) {                                                \
         esp_log_write(ESP_LOG_INFO, TAG,                                      \
                       LOG_COLOR_E "E (%lu) \t\t\t[ERR %d]\n" LOG_RESET_COLOR, \
                       esp_log_timestamp(), __err_rc);                         \
         return __err_rc;                                                      \
      }                                                                        \
      esp_log_write(ESP_LOG_INFO, TAG,                                         \
                    LOG_COLOR_I "I (%lu) \t\t\t[OK]\n" LOG_RESET_COLOR,        \
                    esp_log_timestamp());                                      \
   } while (0)

#define MIN(a, b)                                                              \
   ({                                                                          \
      __typeof__(a) _a = (a);                                                  \
      __typeof__(b) _b = (b);                                                  \
      _a < _b ? _a : _b;                                                       \
   })
