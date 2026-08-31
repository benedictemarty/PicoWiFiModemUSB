/*
 * Host-side regression test for the AT&W (write settings to NVRAM) fix.
 *
 * Background (forum.defence-force.org t=2894, reported by ibisum): "AT&W hangs
 * the PicoW and requires a forced reset." Root cause: while the RP2040 flash is
 * being erased/programmed, XIP is unavailable, so the cyw43 background WiFi IRQ
 * — which executes from flash — deadlocks the chip. The fix (src/lfs.c) wraps
 * every flash erase/program in save_and_disable_interrupts()/restore_interrupts().
 *
 * This test compiles the REAL src/lfs.c and the REAL LittleFS against a RAM-
 * backed flash stub, then drives the real writeSettings()/readSettings() path.
 * It asserts two things that must always hold:
 *   1. INVARIANT — every flash_range_erase()/flash_range_program() runs with
 *      interrupts masked. If the guard is ever removed, the stub sees an
 *      unmasked flash op and the test fails (pins the AT&W deadlock fix).
 *   2. FUNCTIONAL — a settings blob written with writeSettings() reads back
 *      identically with readSettings() (the AT&W happy path still works), and
 *      the flash path was actually exercised (>0 flash ops), and every
 *      disable/restore is balanced.
 *
 * No hardware required. Build/run: see validation/host-tests/run.sh
 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Flash geometry + interrupt-mask stubs (define hosttest_flash / the four
 * functions the test implements below). */
#include "hardware/flash.h"
#include "hardware/sync.h"

/* Pull in the real firmware declarations (initLFS/readSettings/writeSettings
 * and SETTINGS_T) via the project header, resolved against the host stubs. */
#include "lfs.h"

/* ── RAM-backed flash + interrupt-mask model shared with the stubs ────────── */

uint8_t hosttest_flash[PICO_FLASH_SIZE_BYTES];

static int      g_irq_masked   = 0;   /* 1 while interrupts are disabled       */
static unsigned g_saves        = 0;   /* save_and_disable_interrupts() count    */
static unsigned g_restores     = 0;   /* restore_interrupts() count             */
static unsigned g_flash_ops    = 0;   /* erase + program calls                  */
static unsigned g_ops_unmasked = 0;   /* flash ops seen with interrupts ENABLED */

uint32_t save_and_disable_interrupts(void) {
   g_saves++;
   g_irq_masked = 1;
   return 1;   /* opaque token; the real SDK returns the saved IRQ state */
}

void restore_interrupts(uint32_t status) {
   (void)status;
   g_restores++;
   g_irq_masked = 0;
}

void flash_range_erase(uint32_t flash_offs, size_t count) {
   g_flash_ops++;
   if (!g_irq_masked) g_ops_unmasked++;          /* AT&W invariant violation */
   if (flash_offs + count <= sizeof(hosttest_flash))
      memset(hosttest_flash + flash_offs, 0xFF, count);   /* NOR erase = all 1s */
}

void flash_range_program(uint32_t flash_offs, const uint8_t *data, size_t count) {
   g_flash_ops++;
   if (!g_irq_masked) g_ops_unmasked++;          /* AT&W invariant violation */
   if (flash_offs + count <= sizeof(hosttest_flash))
      memcpy(hosttest_flash + flash_offs, data, count);
}

/* ── Test harness ────────────────────────────────────────────────────────── */

static int failures = 0;

#define CHECK(cond, fmt, ...) do { \
  if (cond) { printf("  [PASS] " fmt "\n", ##__VA_ARGS__); } \
  else      { printf("  [FAIL] " fmt "\n", ##__VA_ARGS__); failures++; } \
} while (0)

int main(void) {
   printf("== AT&W / writeSettings regression test ==\n");

   /* Fresh (zeroed) flash → initLFS() formats then mounts. */
   initLFS();

   /* Build a recognisable settings blob and persist it, exactly as AT&W does. */
   SETTINGS_T out;
   for (size_t i = 0; i < sizeof(out); i++)
      ((uint8_t *)&out)[i] = (uint8_t)(i * 7u + 0x2Bu);

   bool wrote = writeSettings(&out);
   CHECK(wrote, "writeSettings() succeeded");

   SETTINGS_T in;
   memset(&in, 0, sizeof(in));
   bool read = readSettings(&in);
   CHECK(read, "readSettings() succeeded");
   CHECK(memcmp(&out, &in, sizeof(out)) == 0,
         "settings round-trip byte-identical (%zu bytes)", sizeof(out));

   /* The whole point: prove the flash path ran AND stayed interrupt-safe. */
   CHECK(g_flash_ops > 0, "flash path exercised (%u erase/program ops)", g_flash_ops);
   CHECK(g_ops_unmasked == 0,
         "every flash op ran with interrupts masked (AT&W deadlock guard)");
   CHECK(g_saves == g_restores,
         "interrupt save/restore balanced (%u/%u)", g_saves, g_restores);

   printf("\n%s\n", failures ? "AT&W test FAILED." : "AT&W test passed.");
   return failures ? 1 : 0;
}
