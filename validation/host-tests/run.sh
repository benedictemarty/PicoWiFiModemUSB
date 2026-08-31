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
  -DLFS_NO_MALLOC=1 -DLFS_NAME_MAX=64 \
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
echo "All host tests passed."
