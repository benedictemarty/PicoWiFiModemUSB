// settings_migrate.h — accept or migrate the settings read from LittleFS.
// Host-tested (validation/host-tests/test_settings_migrate.c).
#ifndef _SETTINGS_MIGRATE_H
   #define _SETTINGS_MIGRATE_H
   #include "types.h"

   // Returns true when *s is usable (current, or migrated in RAM), false when
   // the caller must load factory defaults. 0.3.x settings share the current
   // layout; they are kept (WiFi credentials included) and certificate
   // verification is switched ON, the new default — the old stored value came
   // from the former insecure default and cannot be told apart from an explicit
   // AT$CV0. The migrated record is written back only by the next AT&W.
   static inline bool migrateSettings(SETTINGS_T *s) {
      if( s->magicNumber == MAGIC_NUMBER ) {
         return true;
      }
      if( s->magicNumber == MAGIC_NUMBER_V03 ) {
         s->tlsVerify = true;
         s->magicNumber = MAGIC_NUMBER;
         return true;
      }
      return false;
   }
#endif
