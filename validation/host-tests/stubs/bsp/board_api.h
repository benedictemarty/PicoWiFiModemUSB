/* Host-test stub: only declares what usb_descriptors.c references. */
#ifndef _HOSTTEST_BOARD_API_H_
#define _HOSTTEST_BOARD_API_H_
#include <stdint.h>
#include <stddef.h>
static inline size_t board_usb_get_serial(uint16_t desc_str[], size_t max_chars) {
  (void)desc_str; (void)max_chars; return 0;
}
#endif
