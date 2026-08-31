/* Host-test stub: types.h includes several lwip headers but SETTINGS_T uses no
 * lwip type. Provide the minimum the later TCP_CLIENT_T/TCP_SERVER_T structs
 * need (ip_addr_t as a value member, altcp_pcb only as a pointer). */
#ifndef HOSTTEST_LWIP_DNS_H
#define HOSTTEST_LWIP_DNS_H
#include <stdint.h>

typedef struct hosttest_ip_addr { uint32_t addr; } ip_addr_t;
struct altcp_pcb;   /* incomplete: only used through pointers */

#endif
