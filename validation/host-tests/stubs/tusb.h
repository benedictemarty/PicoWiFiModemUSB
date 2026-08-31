/* Host-test stub for tusb.h: pulls only the lightweight descriptor headers
 * (no OS / no device stack), enough to compile src/usb_descriptors.c on host. */
#ifndef _HOSTTEST_TUSB_H_
#define _HOSTTEST_TUSB_H_
#include <string.h>
#include "tusb_config.h"
#include "common/tusb_compiler.h"
#include "common/tusb_types.h"
#include "class/cdc/cdc.h"
#include "device/usbd.h"
#endif
