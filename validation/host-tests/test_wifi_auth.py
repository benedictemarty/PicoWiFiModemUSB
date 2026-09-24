#!/usr/bin/env python3
"""Host-side test: WiFi join mode (v0.4.2).

Every station join must use CYW43_AUTH_WPA2_MIXED_PSK (WPA2 PSK, TKIP + AES
ciphers) when a password is set, so older WPA/WPA2 access points using TKIP are
joined; an empty password stays an open network. Guards against a join path
added or edited back to AES-only."""
import os
import re
import unittest

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'src')


def sources():
    for name in sorted(os.listdir(SRC)):
        if name.endswith(('.c', '.cpp', '.h')):
            with open(os.path.join(SRC, name), encoding='utf-8') as f:
                yield name, f.read()


class WifiAuth(unittest.TestCase):
    def test_joins_use_mixed_mode(self):
        joins = 0
        for name, text in sources():
            for m in re.finditer(r'authMode\s*=\s*([^;]+);', text):
                joins += 1
                self.assertEqual(re.sub(r'\s+', ' ', m.group(1)).strip(),
                                 'settings.wifiPassword[0] ? CYW43_AUTH_WPA2_MIXED_PSK : CYW43_AUTH_OPEN',
                                 name)
        self.assertEqual(joins, 2, 'expected the boot join and the ATC1 join')

    def test_no_aes_only_join(self):
        for name, text in sources():
            self.assertNotIn('CYW43_AUTH_WPA2_AES_PSK', text, name)


if __name__ == '__main__':
    unittest.main(verbosity=1)
