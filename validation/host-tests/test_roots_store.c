/*
 * Host-side test for the built-in trust store lookup (src/roots_store.c, v0.4.0).
 *
 * Real store (certs/roots.pem -> roots_gen.c): every root is found by its own
 * subject, the index is sorted, offsets are contiguous, hashes match. Synthetic
 * store: edge cases (hash collision, two roots sharing a subject, max bound,
 * empty store).
 *
 * No hardware required. Build/run: see validation/host-tests/run.sh
 */
#include <stdio.h>
#include <string.h>

#include "roots_store.h"

static int failures, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { failures++; printf("  [FAIL] l.%d: ", __LINE__); \
                           printf(__VA_ARGS__); printf("\n"); } } while (0)

static int contains(const int *idx, int n, int v)
{
   for (int i = 0; i < n; i++) if (idx[i] == v) return 1;
   return 0;
}

static void real_store(void)
{
   const struct roots_store *s = &roots_store;
   CHECK(s->count >= 100, "real store: %d roots", s->count);
   uint32_t off = 0;
   for (int i = 0; i < s->count; i++) {
      const struct roots_entry *r = &s->e[i];
      CHECK(r->der_off == off, "root %d: offset %u, expected %u", i, (unsigned)r->der_off, (unsigned)off);
      off += r->der_len;
      CHECK(i == 0 || s->e[i - 1].hash <= r->hash, "index not sorted at %d", i);
      const uint8_t *subj = s->der + r->der_off + r->subj_off;
      CHECK(roots_hash(subj, r->subj_len) == r->hash, "hash of %s", r->name);
      CHECK(s->der[r->der_off] == 0x30, "%s: not a DER SEQUENCE", r->name);
      int idx[4], n = roots_find(s, subj, r->subj_len, idx, 4);
      CHECK(n >= 1 && contains(idx, n, i), "%s not found by its subject", r->name);
      CHECK(roots_index_of(s, s->der + r->der_off) == i, "roots_index_of(%s)", r->name);
      CHECK(roots_index_of(s, s->der + r->der_off + 1) == -1, "roots_index_of inside %s", r->name);
   }
   CHECK(off == s->der_size, "DER size %u, sum %u", (unsigned)s->der_size, (unsigned)off);

   static const uint8_t absent[] = { 0x30, 0x0b, 0x31, 0x09, 0x30, 0x07, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x00 };
   int idx[4];
   CHECK(roots_find(s, absent, sizeof absent, idx, 4) == 0, "absent subject found");
   CHECK(roots_find(s, absent, 0, idx, 4) == 0, "empty subject found");
   CHECK(roots_find(s, NULL, 0, idx, 4) == 0, "NULL subject found");
   CHECK(roots_index_of(s, NULL) == -1, "roots_index_of(NULL)");
   CHECK(roots_index_of(s, s->der + s->der_size) == -1, "roots_index_of past the end");
   CHECK(roots_index_of(NULL, s->der) == -1, "roots_index_of(NULL store)");

   int isrg = -1;
   for (int i = 0; i < s->count; i++) if (!strcmp(s->e[i].name, "ISRG Root X1")) isrg = i;
   CHECK(isrg >= 0, "ISRG Root X1 missing from the store");
}

/* Synthetic store: "certificates" reduced to their subject (subj_off = 0). */
static const uint8_t syn_der[] = "AAAA" "BBBB" "AAAA" "CCCC" "ZZZZ";
static struct roots_entry syn_e[5];
static const struct roots_store syn = { syn_der, 20, syn_e, 5 };

static int syn_at(const char *name)
{
   for (int i = 0; i < 5; i++) if (!strcmp(syn_e[i].name, name)) return i;
   return -1;
}

static void synthetic_store(void)
{
   const uint8_t *A = (const uint8_t *)"AAAA", *B = (const uint8_t *)"BBBB", *C = (const uint8_t *)"CCCC";
   uint32_t hA = roots_hash(A, 4);
   /* A1, B-collision (A's hash forced), A2: same hash; C; Z last */
   const struct roots_entry e[5] = {
      { hA, 0, 4, 0, 4, "A1" }, { hA, 4, 4, 0, 4, "B-collision" }, { hA, 8, 4, 0, 4, "A2" },
      { roots_hash(C, 4), 12, 4, 0, 4, "C" }, { 0xffffffffu, 16, 4, 0, 4, "Z" },
   };
   memcpy(syn_e, e, sizeof e);
   for (int i = 1; i < 5; i++)                       /* stable sort by hash, like roots2c */
      for (int j = i; j > 0 && syn_e[j - 1].hash > syn_e[j].hash; j--) {
         struct roots_entry t = syn_e[j]; syn_e[j] = syn_e[j - 1]; syn_e[j - 1] = t;
      }

   int idx[4];
   int n = roots_find(&syn, A, 4, idx, 4);
   CHECK(n == 2 && idx[0] == syn_at("A1") && idx[1] == syn_at("A2"), "twins A (collision B skipped): n=%d", n);
   CHECK(roots_find(&syn, A, 4, idx, 1) == 1 && idx[0] == syn_at("A1"), "max=1 bound");
   CHECK(roots_find(&syn, A, 4, idx, 0) == 0, "max=0 bound");
   CHECK(roots_find(&syn, A, 3, idx, 4) == 0, "prefix of A accepted");
   CHECK(roots_find(&syn, B, 4, idx, 4) == 0, "B found despite a different hash");
   n = roots_find(&syn, C, 4, idx, 4);
   CHECK(n == 1 && idx[0] == syn_at("C"), "C: n=%d", n);
   CHECK(roots_index_of(&syn, syn_der + 16) == syn_at("Z"), "last DER entry");
   CHECK(roots_find(&(struct roots_store){ syn_der, 0, syn_e, 0 }, A, 4, idx, 4) == 0, "empty store");
   CHECK(roots_hash((const uint8_t *)"", 0) == 0x811c9dc5u, "FNV-1a empty");
   CHECK(roots_hash((const uint8_t *)"foobar", 6) == 0xbf9cf968u, "FNV-1a foobar");
}

int main(void)
{
   printf("== built-in trust store lookup test ==\n");
   real_store();
   synthetic_store();
   printf("  %d checks, %d failure(s)\n", checks, failures);
   printf("\n%s\n", failures ? "Trust store test FAILED." : "Trust store test passed.");
   return failures ? 1 : 0;
}
