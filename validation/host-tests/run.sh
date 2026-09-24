#!/usr/bin/env bash
#
# Host-side unit tests for PicoWiFiModemUSB (no hardware required).
# Compiles the real firmware descriptor source against lightweight tinyusb
# headers and validates USB descriptor consistency.
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# host-tests now live inside the firmware repo (PicoWiFiModemUSB/validation/).
SRC="$HERE/../../src"
TUSB="$SRC/tinyusb/src"
STUBS="$HERE/stubs"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

CC="${CC:-gcc}"

echo "== Building USB descriptor test =="
# Stub include dir (tusb.h, bsp/board_api.h) MUST come before the real tinyusb
# tree so usb_descriptors.c picks up the host stubs instead of the full stack.
"$CC" -std=c11 -Wall -Wextra \
  -I"$STUBS" \
  -I"$SRC" \
  -I"$TUSB" \
  "$HERE/test_usb_descriptors.c" \
  "$SRC/usb_descriptors.c" \
  -o "$OUT/test_usb_descriptors"

echo "== Running USB descriptor test =="
"$OUT/test_usb_descriptors"

echo
# ── AT&W / writeSettings regression test ─────────────────────────────────────
# Compiles the REAL src/lfs.c + LittleFS against a RAM-backed flash stub and
# asserts every flash erase/program runs with interrupts masked (the AT&W
# deadlock fix) plus a writeSettings/readSettings round-trip. Match the firmware
# LittleFS config (LFS_NO_MALLOC, LFS_NAME_MAX=64 — see src/CMakeLists.txt).
STUBS_LFS="$HERE/stubs-lfs"
LFS="$SRC/littlefs"
echo "== Building AT&W / writeSettings test =="
# Stub headers (pico/, lwip/, hardware/) MUST precede $SRC so the host stubs win
# over any same-named real header. LittleFS sources compile with -w: they are
# third-party and not the subject under test.
"$CC" -std=c11 \
  -DLFS_NO_MALLOC=1 -DLFS_NAME_MAX=64 -DFW_VERSION='"host"' \
  -I"$STUBS_LFS" \
  -I"$SRC" \
  -I"$LFS" \
  -w \
  "$HERE/test_lfs_atw.c" \
  "$SRC/lfs.c" \
  "$LFS/lfs.c" \
  "$LFS/lfs_util.c" \
  -o "$OUT/test_lfs_atw"

echo "== Running AT&W / writeSettings test =="
"$OUT/test_lfs_atw"

echo
# ── Built-in trust store (v0.4.0) ────────────────────────────────────────────
# Generator (tools/roots2c.py), indexed lookup (src/roots_store.c), the mbedTLS
# CA callback (src/roots_ca_cb.c) and the 0.3.x → 0.4.0 settings migration.
ROOT="$HERE/../.."
SAN="-fsanitize=address,undefined"
echo "== Running WiFi join mode test =="
python3 "$HERE/test_wifi_auth.py"

echo "== Running version / reproducible-build test =="
python3 "$HERE/test_version.py"

echo "== Running trust store generator test (roots2c.py) =="
python3 "$HERE/test_roots2c.py"

echo "== Building trust store lookup test =="
python3 "$ROOT/tools/roots2c.py" "$ROOT/certs/roots.pem" "$OUT/roots_gen.c"
"$CC" -std=c11 -Wall -Wextra -Werror $SAN -I"$SRC" \
  "$HERE/test_roots_store.c" "$SRC/roots_store.c" "$OUT/roots_gen.c" \
  -o "$OUT/test_roots_store"
echo "== Running trust store lookup test =="
"$OUT/test_roots_store"

echo
echo "== Building settings migration test =="
"$CC" -std=c11 -Wall -Wextra -DFW_VERSION='"host"' \
  -I"$STUBS_LFS" \
  -I"$SRC" \
  "$HERE/test_settings_migrate.c" \
  -o "$OUT/test_settings_migrate"
echo "== Running settings migration test =="
"$OUT/test_settings_migrate"

echo
# The CA callback test builds the SDK's mbedTLS 2.28 on the host. It needs the
# src/pico-sdk submodule with lib/mbedtls (or MBEDTLS_DIR); without it the test
# is SKIPPED, loudly.
MBEDTLS_DIR="${MBEDTLS_DIR:-$SRC/pico-sdk/lib/mbedtls}"
if [ -f "$MBEDTLS_DIR/library/x509_crt.c" ]; then
  echo "== Building mbedTLS for the host (mbedtls_host_config.h) =="
  mkdir -p "$OUT/mbed"
  for f in "$MBEDTLS_DIR"/library/*.c; do
    "$CC" -std=c11 -O1 -w $SAN -I"$HERE" -I"$MBEDTLS_DIR/include" -I"$MBEDTLS_DIR/library" \
      -DMBEDTLS_CONFIG_FILE='"mbedtls_host_config.h"' -c "$f" -o "$OUT/mbed/$(basename "$f" .c).o"
  done
  python3 "$ROOT/tools/roots2c.py" "$HERE/fixtures/store.pem" "$OUT/roots_fixture_gen.c" \
    --symbol roots_fixture 2>/dev/null
  echo "== Building roots_ca_cb test =="
  "$CC" -std=c11 -Wall -Wextra -Werror $SAN -I"$HERE" -I"$SRC" -I"$MBEDTLS_DIR/include" \
    -DMBEDTLS_CONFIG_FILE='"mbedtls_host_config.h"' \
    "$HERE/test_roots_ca_cb.c" "$SRC/roots_ca_cb.c" "$SRC/roots_store.c" \
    "$OUT/roots_gen.c" "$OUT/roots_fixture_gen.c" "$OUT"/mbed/*.o \
    -o "$OUT/test_roots_ca_cb"
  echo "== Running roots_ca_cb test =="
  (cd "$HERE" && "$OUT/test_roots_ca_cb")
else
  echo "*** roots_ca_cb test SKIPPED: mbedTLS not found ($MBEDTLS_DIR) — init src/pico-sdk + lib/mbedtls or set MBEDTLS_DIR ***"
fi

echo
echo "All host tests passed."
