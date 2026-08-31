/* Host-test stub for hardware/flash.h.
 *
 * Models the RP2040 flash geometry with a RAM-backed block store so the REAL
 * src/lfs.c (and the real LittleFS) run unchanged on the host. The flash
 * region is placed at offset 0 (PICO_FLASH_SIZE_BYTES == LFS_DISK_SIZE) so the
 * XIP read base and the program/erase offsets refer to the same bytes.
 *
 * flash_range_program()/flash_range_erase() are implemented in the test file;
 * they assert the AT&W invariant (interrupts must be disabled while flash is
 * busy) and mutate the backing store. */
#ifndef HOSTTEST_HARDWARE_FLASH_H
#define HOSTTEST_HARDWARE_FLASH_H

#include <stdint.h>
#include <stddef.h>

#define FLASH_PAGE_SIZE      256u
#define FLASH_SECTOR_SIZE    4096u

/* LFS_DISK_SIZE in src/lfs.c is 128 * FLASH_SECTOR_SIZE = 512 KiB. Sizing the
 * whole "flash" to exactly that puts the LittleFS region at offset 0. */
#define PICO_FLASH_SIZE_BYTES (128u * FLASH_SECTOR_SIZE)

/* Backing store defined in the test translation unit. */
extern uint8_t hosttest_flash[PICO_FLASH_SIZE_BYTES];
#define XIP_NOCACHE_NOALLOC_BASE ((uintptr_t)hosttest_flash)

void flash_range_erase(uint32_t flash_offs, size_t count);
void flash_range_program(uint32_t flash_offs, const uint8_t *data, size_t count);

#endif /* HOSTTEST_HARDWARE_FLASH_H */
