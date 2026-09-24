#!/usr/bin/env python3
"""Host-side test: version and reproducible-build invariants (v0.4.1).

VERSION (single source, semver) <-> src/CMakeLists.txt <-> CHANGELOG.md <-> git
tag; and nothing in the firmware sources may depend on the build clock
(__DATE__, __TIME__, CMake TIMESTAMP), so the same commit gives the same UF2."""
import os
import re
import subprocess
import unittest

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
SRC = os.path.join(ROOT, 'src')


def read(*p):
    with open(os.path.join(ROOT, *p), encoding='utf-8') as f:
        return f.read()


VERSION = read('VERSION').strip()


def firmware_sources():
    """Project sources only (not the pico-sdk/tinyusb/littlefs submodules)."""
    for name in sorted(os.listdir(SRC)):
        if name == 'CMakeLists.txt' or name.endswith(('.c', '.cpp', '.h')):
            yield name, read('src', name)


class Version(unittest.TestCase):
    def test_semver(self):
        self.assertRegex(VERSION, r'^\d+\.\d+\.\d+$')
        self.assertEqual(read('VERSION'), VERSION + '\n')

    def test_cmake_reads_version(self):
        c = read('src', 'CMakeLists.txt')
        self.assertIn('file(STRINGS ${CMAKE_CURRENT_LIST_DIR}/../VERSION FW_VERSION', c)
        self.assertIn('FW_VERSION=\\"${FW_VERSION}\\"', c)

    def test_no_hard_coded_version(self):
        for name, text in firmware_sources():
            self.assertIsNone(re.search(r'#\s*define\s+FW_VERSION\s', text), name)

    def test_no_build_clock(self):
        for name, text in firmware_sources():
            for bad in ('__DATE__', '__TIME__', 'string(TIMESTAMP'):
                self.assertFalse(bad in text, '%s uses %s: build not reproducible' % (name, bad))

    def test_changelog(self):
        heads = re.findall(r'^## \[(\d+\.\d+\.\d+)\] — (\d{4}-\d{2}-\d{2})', read('CHANGELOG.md'), re.M)
        self.assertTrue(heads, 'no released version in CHANGELOG.md')
        self.assertEqual(heads[0][0], VERSION, 'CHANGELOG: latest release must match VERSION')
        nums = [tuple(map(int, v.split('.'))) for v, _ in heads]
        self.assertEqual(nums, sorted(nums, reverse=True), 'CHANGELOG versions not decreasing')
        self.assertEqual(len(nums), len(set(nums)), 'duplicate version in CHANGELOG')

    def test_head_tag(self):
        r = subprocess.run(['git', 'tag', '--points-at', 'HEAD', '--list', 'v[0-9]*'],
                           cwd=ROOT, capture_output=True, text=True)
        if r.returncode != 0:
            self.skipTest('git unavailable')
        for tag in r.stdout.split():
            self.assertEqual(tag, 'v' + VERSION, 'HEAD tagged %s but VERSION = %s' % (tag, VERSION))


if __name__ == '__main__':
    unittest.main(verbosity=1)
