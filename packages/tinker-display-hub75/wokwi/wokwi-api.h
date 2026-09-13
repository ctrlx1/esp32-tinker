#include <stdint.h>
#include <stdbool.h>

#ifndef WOKWI_API_H
#define WOKWI_API_H

enum pin_value { LOW = 0, HIGH = 1 };

enum pin_mode {
  INPUT = 0,
  OUTPUT = 1,
  INPUT_PULLUP = 2,
  INPUT_PULLDOWN = 3,
  ANALOG = 4,
  OUTPUT_LOW = 16,
  OUTPUT_HIGH = 17,
};

enum edge { RISING = 1, FALLING = 2, BOTH = 3 };

int __attribute__((export_name("__wokwi_api_version_1")))
__attribute__((weak)) __wokwi_api_version_1(void) {
  return 1;
}

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t pin_t;
#define NO_PIN ((pin_t) - 1)

typedef struct {
  void *user_data;
  uint32_t edge;
  void (*pin_change)(void *user_data, pin_t pin, uint32_t value);
} pin_watch_config_t;

extern __attribute__((export_name("chipInit"))) void chip_init(void);

extern __attribute__((import_name("pinInit"))) pin_t pin_init(const char *name,
                                                              uint32_t mode);
extern __attribute__((import_name("pinRead"))) uint32_t pin_read(pin_t pin);
extern __attribute__((import_name("pinWrite"))) void pin_write(pin_t pin,
                                                               uint32_t value);
extern __attribute__((import_name("pinWatch"))) bool
pin_watch(pin_t pin, const pin_watch_config_t *config);
extern __attribute__((import_name("pinWatchStop"))) void
pin_watch_stop(pin_t pin);
extern __attribute__((import_name("pinMode"))) void pin_mode(pin_t pin,
                                                             uint32_t value);

typedef uint32_t buffer_t;
extern __attribute__((import_name("framebufferInit"))) buffer_t
framebuffer_init(uint32_t *pixel_width, uint32_t *pixel_height);
extern __attribute__((import_name("bufferRead"))) void
buffer_read(buffer_t buffer, uint32_t offset, uint8_t *data, uint32_t data_len);
extern __attribute__((import_name("bufferWrite"))) void
buffer_write(buffer_t buffer, uint32_t offset, uint8_t *data,
             uint32_t data_len);

#ifdef __cplusplus
}
#endif

#endif /* WOKWI_API_H */
