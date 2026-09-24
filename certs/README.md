# Built-in trust store

`roots.pem` is compiled into the firmware by `tools/roots2c.py` → `roots_gen.c`
at build time: concatenated DER certificates **in flash** + an index sorted by
the FNV-1a hash of each subject (`src/roots_store.c`). No root is parsed up
front: during the TLS handshake mbedTLS calls `roots_ca_cb`
(`src/roots_ca_cb.c`), which parses only the roots whose subject equals the
issuer being looked up. RAM use therefore does not depend on the store size.

Used when certificate verification is on (`AT$CV1`, the default) and no CA was
uploaded with `AT$CA=` (an uploaded CA replaces this store).

## Current store

| Field | Value |
|---|---|
| Content | Mozilla root store ("websites" trust), as shipped by the Ubuntu `ca-certificates` package |
| Source | `/etc/ssl/certs/ca-certificates.crt`, `ca-certificates` **20250419**, copied **2026-09-24** |
| File SHA-256 | `693f769039db98ce5d24b50e5053f731401c4374d1f43ac1699c4bff2968ad47` |
| Roots | 150 (RSA 4096: 65, RSA 2048: 42, EC P-384: 39, EC P-256: 4), all kept |
| Flash | 159,591 bytes of DER + index |

`roots2c.py` excludes (and lists at build time) keys it treats as unsupported:
RSA below 2048 bits, curves other than P-256 / P-384, other algorithms, and
exact duplicates. None is excluded from the current store. (This firmware's
mbedTLS also enables P-521; the Mozilla store has no P-521 root.)

## Updating the store

1. Replace `roots.pem` (same origin preferably: an up-to-date `ca-certificates`,
   or the official Mozilla export).
2. Record the version, date and SHA-256 (`sha256sum roots.pem`) above.
3. Run `validation/host-tests/run.sh`: it requires ISRG Root X1 / X2, DigiCert
   Global Root G2, USERTrust ECC, and verifies the real chains captured in
   `validation/host-tests/fixtures/real_*.pem` (refresh them with
   `validation/host-tests/fixtures/gen.sh --real`).
4. Rebuild, flash, run `validation/validate_trust_store.py`, log it in the
   CHANGELOG.

The same store and generator are used by the Neo6502picowifi modem firmware.
