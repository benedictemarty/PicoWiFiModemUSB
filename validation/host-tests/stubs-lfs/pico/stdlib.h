/* Host-test stub for pico/stdlib.h.
 *
 * Just enough for src/types.h to compile on the host: standard integer/bool
 * types plus the uart_parity_t enum that SETTINGS_T stores. No hardware. */
#ifndef HOSTTEST_PICO_STDLIB_H
#define HOSTTEST_PICO_STDLIB_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

typedef enum uart_parity {
   UART_PARITY_NONE = 0,
   UART_PARITY_EVEN,
   UART_PARITY_ODD,
} uart_parity_t;

#endif /* HOSTTEST_PICO_STDLIB_H */
