/*
 * mbedtls_host_config.h — the SDK's mbedTLS 2.28 built on the host to test
 * roots_ca_cb (v0.4.0): same X.509 subset as src/mbedtls_config.h (RSA,
 * P-256/P-384/P-521, SHA-1/2), no TLS, no hardware entropy; dates checked by
 * mbedTLS (gmtime_r is fine on a PC); replaceable allocator to measure memory.
 */
#ifndef MBEDTLS_HOST_CONFIG_H
#define MBEDTLS_HOST_CONFIG_H

#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_HAVE_TIME_DATE
#define MBEDTLS_FS_IO
#define MBEDTLS_ERROR_C

#define MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_OID_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C

#define MBEDTLS_BIGNUM_C
#define MBEDTLS_RSA_C
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_ECP_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_DP_SECP521R1_ENABLED

#define MBEDTLS_MD_C
#define MBEDTLS_SHA1_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA512_C
#define MBEDTLS_HMAC_DRBG_C   /* ecp.c (2.28): internal DRBG for point-multiplication blinding */

#include "mbedtls/check_config.h"

#endif
