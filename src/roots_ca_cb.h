// roots_ca_cb.h — mbedTLS "trusted CA on demand" callback over the built-in
// store. Install with mbedtls_ssl_conf_ca_cb(conf, roots_ca_cb, (void *)&roots_store).
#ifndef ROOTS_CA_CB_H
#define ROOTS_CA_CB_H
#include "mbedtls/x509_crt.h"
#include "roots_store.h"

#ifdef __cplusplus
extern "C" {
#endif

// Max roots sharing one subject returned per call (key rollover).
#define ROOTS_MAX_CANDIDATES 4

// ctx = const struct roots_store *. Candidates = the roots whose subject equals
// child's issuer, parsed without copying (the DER stays in flash); none →
// *candidates = NULL and mbedTLS concludes "not trusted". mbedTLS frees the
// list. Returns 0, or MBEDTLS_ERR_X509_ALLOC_FAILED (fatal).
int roots_ca_cb(void *ctx, mbedtls_x509_crt const *child, mbedtls_x509_crt **candidates);

#ifdef __cplusplus
}
#endif

#endif
