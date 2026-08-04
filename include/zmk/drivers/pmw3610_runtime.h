#ifndef ZMK_DRIVERS_PMW3610_RUNTIME_H_
#define ZMK_DRIVERS_PMW3610_RUNTIME_H_

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

int pmw3610_set_scroll_layers(const struct device *dev, uint32_t mask, bool persist);
int pmw3610_get_scroll_layers(const struct device *dev, uint32_t *out);

#ifdef __cplusplus
}
#endif

#endif /* ZMK_DRIVERS_PMW3610_RUNTIME_H_ */
