/*
 * Host-side regression test for the USB configuration descriptor.
 *
 * Background (forum.defence-force.org p34901): PicoWiFiModemUSB v0.2.0 created no
 * COM port on Windows. The configuration descriptor advertised two CDC functions
 * (4 interfaces) while CFG_TUD_CDC stayed 1, so wTotalLength (computed from
 * CFG_TUD_CDC) was 75 bytes but bNumInterfaces said 4. Windows rejected the
 * inconsistent descriptor; Linux tolerated it.
 *
 * This test compiles the REAL src/usb_descriptors.c (via lightweight stubs for
 * tusb.h / bsp/board_api.h) and asserts the two invariants that must always hold:
 *   1. wTotalLength == sizeof(desc_fs_configuration)
 *   2. bNumInterfaces == number of INTERFACE descriptors actually present
 *
 * Build/run: see validation/host-tests/run.sh
 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

/* The descriptor array under test lives in the real firmware source. */
extern uint8_t const desc_fs_configuration[];

/* tusb_types.h gives us TUSB_DESC_* values; include the stub umbrella. */
#include "tusb.h"

static int failures = 0;

#define CHECK(cond, fmt, ...) do { \
  if (cond) { printf("  [PASS] " fmt "\n", ##__VA_ARGS__); } \
  else      { printf("  [FAIL] " fmt "\n", ##__VA_ARGS__); failures++; } \
} while (0)

int main(void) {
  const uint8_t *d = desc_fs_configuration;

  /* Standard configuration descriptor layout:
   * [0]=bLength(9) [1]=bDescriptorType(CONFIGURATION)
   * [2..3]=wTotalLength (LE) [4]=bNumInterfaces [5]=bConfigurationValue ... */
  CHECK(d[0] == 9, "config bLength == 9 (got %u)", d[0]);
  CHECK(d[1] == TUSB_DESC_CONFIGURATION,
        "config bDescriptorType == CONFIGURATION (got 0x%02X)", d[1]);

  uint16_t wTotalLength = (uint16_t)(d[2] | (d[3] << 8));
  uint8_t  bNumInterfaces = d[4];

  /* Walk the whole descriptor blob up to the declared wTotalLength and count the
   * standalone INTERFACE descriptors (excludes IAD and CS_INTERFACE). */
  unsigned offset = 0;
  unsigned itf_count = 0;
  unsigned walked_len = 0;
  while (offset + 2 <= wTotalLength) {
    uint8_t bLength = d[offset];
    uint8_t bType   = d[offset + 1];
    if (bLength == 0) break; /* malformed: avoid infinite loop */
    if (bType == TUSB_DESC_INTERFACE) itf_count++;
    offset += bLength;
    walked_len = offset;
  }

  CHECK(walked_len == wTotalLength,
        "descriptor chain length walks exactly to wTotalLength (walked=%u, wTotalLength=%u)",
        walked_len, wTotalLength);

  CHECK(bNumInterfaces == itf_count,
        "bNumInterfaces (%u) matches actual INTERFACE descriptors (%u)",
        bNumInterfaces, itf_count);

  /* The PicoWiFiModemUSB is a single AT-command CDC modem: exactly 2 interfaces
   * (CDC control + CDC data). This is the configuration validated against Windows. */
  CHECK(bNumInterfaces == 2,
        "single-CDC modem advertises exactly 2 interfaces (got %u)", bNumInterfaces);

  printf("\n%s (%d failure%s)\n",
         failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
         failures, failures == 1 ? "" : "s");
  return failures == 0 ? 0 : 1;
}
