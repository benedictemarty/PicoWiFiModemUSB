/* Host-test stub for hardware/sync.h.
 *
 * save_and_disable_interrupts()/restore_interrupts() are implemented in the
 * test file so it can track whether interrupts are currently masked and assert
 * the AT&W deadlock invariant from inside the flash stubs. */
#ifndef HOSTTEST_HARDWARE_SYNC_H
#define HOSTTEST_HARDWARE_SYNC_H

#include <stdint.h>

uint32_t save_and_disable_interrupts(void);
void     restore_interrupts(uint32_t status);

#endif /* HOSTTEST_HARDWARE_SYNC_H */
