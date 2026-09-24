/*
 * Host-side test for roots_ca_cb (src/roots_ca_cb.c, v0.4.0) against the SDK's
 * mbedTLS 2.28 built on the PC (mbedtls_host_config.h).
 *
 * Local chains (fixtures/gen.sh): 3-level chain, two roots sharing a subject,
 * unknown root, wrong host name. Real captured chains (DigiCert, Sectigo,
 * Let's Encrypt; `fixtures/gen.sh --real` refreshes them — their leaves expire,
 * so EXPIRED/FUTURE are ignored for them). Also: allocation failure, no leak,
 * and heap peak of an on-demand verification vs parsing every root up front.
 * Sizes are measured on a 64-bit PC: an order of magnitude, not RP2040 bytes.
 *
 * No hardware required. Build/run: see validation/host-tests/run.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "roots_ca_cb.h"
#include "mbedtls/platform.h"

extern const struct roots_store roots_fixture;   /* fixtures/store.pem */

static int failures, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { failures++; printf("  [FAIL] l.%d: ", __LINE__); \
                           printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ── counting allocator ──────────────────────────────────────────────────── */
static size_t cur, peak;
static int fail_allocs;          /* > 0: the next allocations fail */

static void *count_calloc(size_t n, size_t sz)
{
   if (fail_allocs) return NULL;
   if (sz && n > (size_t)-1 / sz - 16) return NULL;
   size_t *p = calloc(1, n * sz + 16);
   if (!p) return NULL;
   p[0] = n * sz;
   cur += n * sz;
   if (cur > peak) peak = cur;
   return (char *)p + 16;
}

static void count_free(void *q)
{
   if (!q) return;
   size_t *p = (size_t *)((char *)q - 16);
   cur -= p[0];
   free(p);
}

/* ── verification ────────────────────────────────────────────────────────── */
struct seen { const struct roots_store *s; int root; };

static int vrfy(void *ctx, mbedtls_x509_crt *crt, int depth, uint32_t *flags)
{
   (void)depth; (void)flags;
   struct seen *v = ctx;
   int r = roots_index_of(v->s, crt->raw.p);
   if (r >= 0) v->root = r;
   return 0;
}

/* Verify the chain in file; returns the mbedTLS flags (0xffffffff when mbedTLS
   fails without a flag), *root = store root that anchored the chain. */
static uint32_t verify(const struct roots_store *s, const char *file, const char *host, int *root, size_t *pk)
{
   size_t base = cur;
   mbedtls_x509_crt chain;
   mbedtls_x509_crt_init(&chain);
   int ret = mbedtls_x509_crt_parse_file(&chain, file);
   CHECK(ret == 0, "reading %s: -0x%x", file, (unsigned)-ret);
   uint32_t flags = 0;
   struct seen v = { s, -1 };
   size_t before = cur;
   peak = cur;
   ret = mbedtls_x509_crt_verify_with_ca_cb(&chain, roots_ca_cb, (void *)s, &mbedtls_x509_crt_profile_default,
                                            host, &flags, vrfy, &v);
   if (pk) *pk = peak - before;
   mbedtls_x509_crt_free(&chain);   /* also frees the RSA cache (RN) set on chain keys */
   CHECK(cur == base, "%s: %zu bytes not freed", file, cur - base);
   if (root) *root = v.root;
   if (ret != 0 && flags == 0) return 0xffffffffu;
   return flags;
}

static const char *name(const struct roots_store *s, int i) { return i >= 0 ? s->e[i].name : "(none)"; }

static void fixtures(void)
{
   const struct roots_store *s = &roots_fixture;
   int r;
   uint32_t f = verify(s, "fixtures/chain_a.pem", "test.example", &r, NULL);
   CHECK(f == 0 && r >= 0 && !strcmp(name(s, r), "Test Root A"), "3-level chain: flags 0x%x, root %s", f, name(s, r));

   f = verify(s, "fixtures/chain_a.pem", "other.example", &r, NULL);
   CHECK(f & MBEDTLS_X509_BADCERT_CN_MISMATCH, "wrong host name accepted (flags 0x%x)", f);
   CHECK(!(f & MBEDTLS_X509_BADCERT_NOT_TRUSTED), "wrong host name: chain should still be trusted (0x%x)", f);

   f = verify(s, "fixtures/chain_t.pem", "twin.example", &r, NULL);
   CHECK(f == 0 && r >= 0 && !strcmp(name(s, r), "Test Twin Root"), "twin roots: flags 0x%x, root %s", f, name(s, r));

   f = verify(s, "fixtures/chain_x.pem", "x.example", &r, NULL);
   CHECK(f & MBEDTLS_X509_BADCERT_NOT_TRUSTED, "unknown root accepted (flags 0x%x)", f);
   CHECK(r == -1, "unknown root: store root %s reported", name(s, r));

   f = verify(&roots_store, "fixtures/chain_a.pem", "test.example", &r, NULL);
   CHECK(f & MBEDTLS_X509_BADCERT_NOT_TRUSTED, "test root accepted by the real store (flags 0x%x)", f);

   /* twin leaf: the callback offers both roots sharing the subject */
   mbedtls_x509_crt leaf, *cand = NULL;
   mbedtls_x509_crt_init(&leaf);
   CHECK(mbedtls_x509_crt_parse_file(&leaf, "fixtures/chain_t.pem") == 0, "reading chain_t");
   CHECK(roots_ca_cb((void *)s, &leaf, &cand) == 0 && cand && cand->next && !cand->next->next,
         "twins: 2 candidates expected");
   mbedtls_x509_crt_free(cand); mbedtls_free(cand);

   fail_allocs = 1;                               /* allocation failure: fatal, nothing returned */
   cand = (mbedtls_x509_crt *)&cand;
   CHECK(roots_ca_cb((void *)s, &leaf, &cand) == MBEDTLS_ERR_X509_ALLOC_FAILED && cand == NULL, "allocation failure");
   fail_allocs = 0;
   mbedtls_x509_crt_free(&leaf);

   mbedtls_x509_crt_init(&leaf);                  /* issuer absent: no candidate, no error */
   CHECK(mbedtls_x509_crt_parse_file(&leaf, "fixtures/chain_x.pem") == 0, "reading chain_x");
   cand = (mbedtls_x509_crt *)&cand;
   CHECK(roots_ca_cb((void *)s, &leaf, &cand) == 0 && cand == NULL, "issuer absent");
   mbedtls_x509_crt_free(&leaf);
}

static size_t real_chains(void)
{
   static const struct { const char *file, *host, *root; } t[] = {
      { "fixtures/real_www.digicert.com.pem", "www.digicert.com", "DigiCert Global Root G2" },
      { "fixtures/real_github.com.pem", "github.com", "Sectigo Public Server Authentication Root E46" },
      { "fixtures/real_mimuma.pl.pem", "mimuma.pl", "ISRG Root X1" },
   };
   size_t worst = 0;
   for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
      int r;
      size_t pk;
      uint32_t f = verify(&roots_store, t[i].file, t[i].host, &r, &pk);
      f &= ~(uint32_t)(MBEDTLS_X509_BADCERT_EXPIRED | MBEDTLS_X509_BADCERT_FUTURE);
      CHECK(f == 0, "%s: flags 0x%x", t[i].host, f);
      CHECK(r >= 0 && !strcmp(name(&roots_store, r), t[i].root), "%s: root %s, expected %s",
            t[i].host, name(&roots_store, r), t[i].root);
      printf("  %-18s -> %-46s heap peak during verification: %zu B\n", t[i].host, name(&roots_store, r), pk);
      if (pk > worst) worst = pk;
   }
   return worst;
}

/* The alternative: every root parsed up front. */
static void memory(size_t on_demand_peak)
{
   const struct roots_store *s = &roots_store;
   mbedtls_x509_crt all;
   size_t before = cur;
   mbedtls_x509_crt_init(&all);
   for (int i = 0; i < s->count; i++)
      CHECK(mbedtls_x509_crt_parse_der(&all, s->der + s->e[i].der_off, s->e[i].der_len) == 0, "parsing %s", s->e[i].name);
   size_t copy = cur - before;
   mbedtls_x509_crt_free(&all);
   CHECK(cur == before, "leak after parsing every root");
   printf("  %d roots parsed up front: %zu B; on demand: peak %zu B\n", s->count, copy, on_demand_peak);
   CHECK(on_demand_peak * 10 < copy, "memory gain too small: %zu vs %zu", on_demand_peak, copy);
}

int main(void)
{
   printf("== roots_ca_cb vs mbedTLS test ==\n");
   mbedtls_platform_set_calloc_free(count_calloc, count_free);
   fixtures();
   size_t pk = real_chains();
   memory(pk);
   CHECK(cur == 0, "%zu bytes still allocated at the end", cur);
   printf("  %d checks, %d failure(s)\n", checks, failures);
   printf("\n%s\n", failures ? "roots_ca_cb test FAILED." : "roots_ca_cb test passed.");
   return failures ? 1 : 0;
}
