/*
 * Host-side test for migrateSettings() (src/settings_migrate.h, v0.4.0).
 *
 * 0.4.0 turns certificate verification ON by default. Units upgraded from
 * 0.3.x hold settings saved with the former insecure default (tlsVerify = 0)
 * under MAGIC_NUMBER_V03. They must keep every setting (WiFi credentials
 * above all) and come out with verification ON; current records must be left
 * untouched (an explicit AT$CV0 + AT&W stays off); anything else must fall
 * back to factory defaults.
 *
 * No hardware required. Build/run: see validation/host-tests/run.sh
 */
#include <stdio.h>
#include <string.h>

#include "settings_migrate.h"

static int failures;
#define CHECK(cond, fmt, ...) do { \
  if (cond) { printf("  [PASS] " fmt "\n", ##__VA_ARGS__); } \
  else      { printf("  [FAIL] " fmt "\n", ##__VA_ARGS__); failures++; } \
} while (0)

static SETTINGS_T sample(uint16_t magic, bool verify)
{
   SETTINGS_T s;
   memset(&s, 0xA5, sizeof s);
   s.magicNumber = magic;
   strcpy(s.ssid, "HomeNet");
   strcpy(s.wifiPassword, "secret-pass");
   s.serialSpeed = 115200;
   s.tzOffsetMin = 120;
   s.tlsVerify = verify;
   return s;
}

int main(void)
{
   printf("== settings migration test (0.3.x -> 0.4.0) ==\n");

   CHECK(MAGIC_NUMBER != MAGIC_NUMBER_V03, "new magic differs from the 0.3.x one");

   /* 0.3.x record with the old insecure default: kept, verification turned on */
   SETTINGS_T old = sample(MAGIC_NUMBER_V03, false), want = old;
   bool usable = migrateSettings(&old);
   CHECK(usable, "0.3.x settings accepted");
   CHECK(old.tlsVerify, "0.3.x settings: verification switched ON");
   CHECK(old.magicNumber == MAGIC_NUMBER, "0.3.x settings: magic upgraded");
   want.tlsVerify = true;
   want.magicNumber = MAGIC_NUMBER;
   CHECK(memcmp(&old, &want, sizeof old) == 0, "0.3.x settings: every other field preserved (WiFi, speed, TZ)");

   /* current record, explicit opt-out: untouched */
   SETTINGS_T cur = sample(MAGIC_NUMBER, false), before = cur;
   CHECK(migrateSettings(&cur), "current settings accepted");
   CHECK(memcmp(&cur, &before, sizeof cur) == 0, "current settings untouched (AT$CV0 + AT&W respected)");
   cur = sample(MAGIC_NUMBER, true);
   CHECK(migrateSettings(&cur) && cur.tlsVerify, "current settings with verification on: unchanged");

   /* blank or foreign flash: factory defaults required */
   SETTINGS_T blank;
   memset(&blank, 0xFF, sizeof blank);
   CHECK(!migrateSettings(&blank), "erased flash (0xFFFF) rejected -> factory defaults");
   SETTINGS_T zero;
   memset(&zero, 0, sizeof zero);
   CHECK(!migrateSettings(&zero), "zeroed record rejected -> factory defaults");
   SETTINGS_T older = sample(0x5679, false);
   CHECK(!migrateSettings(&older), "pre-0.3.1 magic (other layout) rejected -> factory defaults");

   printf("\n%s\n", failures ? "Settings migration test FAILED." : "Settings migration test passed.");
   return failures ? 1 : 0;
}
