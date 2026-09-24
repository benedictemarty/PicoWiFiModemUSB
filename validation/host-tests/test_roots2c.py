#!/usr/bin/env python3
"""Host-side test for tools/roots2c.py (built-in trust store, v0.4.0).

Subjects, issuers, labels and key types extracted by the minimal DER parser
are cross-checked against the `cryptography` library when it is installed
(otherwise those cross-checks are reported as skipped)."""
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tools'))
import roots2c  # noqa: E402

try:
    from cryptography import x509
    from cryptography.hazmat.primitives.asymmetric import ec, rsa
    from cryptography.x509.oid import NameOID
except ImportError:
    x509 = None

STORE = os.path.join(HERE, '..', '..', 'certs', 'roots.pem')
FIX = os.path.join(HERE, 'fixtures', 'store.pem')


def read(p):
    with open(p, 'rb') as f:
        return f.read()


class Generator(unittest.TestCase):
    def test_fnv1a_vectors(self):
        self.assertEqual(roots2c.fnv1a32(b''), 0x811c9dc5)
        self.assertEqual(roots2c.fnv1a32(b'a'), 0xe40c292c)
        self.assertEqual(roots2c.fnv1a32(b'foobar'), 0xbf9cf968)

    def test_real_store(self):
        kept, excluded = roots2c.build(read(STORE))
        self.assertEqual(len(kept) + len(excluded), len(roots2c.load(read(STORE))))
        self.assertGreaterEqual(len(kept), 100)
        names = {r['name'] for r in kept}
        for n in ('ISRG Root X1', 'ISRG Root X2', 'DigiCert Global Root G2',
                  'USERTrust ECC Certification Authority'):
            self.assertIn(n, names)

    def test_sorted_index_contiguous_offsets(self):
        kept, _ = roots2c.build(read(STORE))
        self.assertEqual([r['hash'] for r in kept], sorted(r['hash'] for r in kept))
        off = 0
        for r in kept:
            self.assertEqual(r['off'], off)
            off += len(r['der'])
            self.assertLess(len(r['der']), 1 << 16)

    def test_roots_are_self_signed(self):
        # subject == issuer for a root: cross-checks the parsing of two fields
        for der in roots2c.load(read(STORE)):
            _, subj, iss, _, _ = roots2c.parse_cert(der)
            self.assertEqual(subj, iss)

    def test_subject_at_announced_offset(self):
        for r in roots2c.build(read(STORE))[0]:
            d = r['der']
            self.assertEqual(roots2c.fnv1a32(d[r['subj_off']:r['subj_off'] + r['subj_len']]), r['hash'])

    def test_fixture_exclusions(self):
        kept, excluded = roots2c.build(read(FIX))
        self.assertEqual(sorted(r['name'] for r in kept), ['Test Root A', 'Test Twin Root', 'Test Twin Root'])
        why = sorted(w for _, w in excluded)
        self.assertEqual(len(why), 3)
        self.assertIn('doublon', why)
        self.assertTrue(any('RSA-1024' in w for w in why))
        self.assertTrue(any('1.3.132.0.35' in w for w in why))       # P-521
        twins = [r for r in kept if r['name'] == 'Test Twin Root']
        self.assertEqual(twins[0]['hash'], twins[1]['hash'])

    def test_c_output(self):
        import subprocess
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            out = os.path.join(d, 'r.c')
            subprocess.run([sys.executable, os.path.join(HERE, '..', '..', 'tools', 'roots2c.py'), FIX, out,
                            '--symbol', 'roots_x'], check=True, capture_output=True)
            with open(out) as f:
                c = f.read()
        self.assertIn('const struct roots_store roots_x = { roots_x_der, 1629u, roots_x_entries, 3 };', c)
        self.assertIn('depuis store.pem', c)


@unittest.skipIf(x509 is None, 'cryptography missing: cross-checks skipped')
class CrossCheck(unittest.TestCase):
    def test_subjects_issuers_labels_keys(self):
        for der in roots2c.load(read(STORE)):
            ref = x509.load_der_x509_certificate(der)
            off, subj, iss, key, name = roots2c.parse_cert(der)
            self.assertEqual(subj, ref.subject.public_bytes())
            self.assertEqual(iss, ref.issuer.public_bytes())
            self.assertEqual(der[off:off + len(subj)], subj)
            k = ref.public_key()
            if isinstance(k, rsa.RSAPublicKey):
                self.assertEqual(key, 'RSA-%d' % k.key_size)
            elif isinstance(k, ec.EllipticCurvePublicKey):
                self.assertEqual(key, {'secp256r1': 'EC-P-256', 'secp384r1': 'EC-P-384'}[k.curve.name])
            for oid in (NameOID.COMMON_NAME, NameOID.ORGANIZATIONAL_UNIT_NAME, NameOID.ORGANIZATION_NAME):
                a = ref.subject.get_attributes_for_oid(oid)
                if a:
                    want = ''.join(ch if ' ' <= ch <= '~' else '?' for ch in a[0].value)
                    self.assertEqual(name, want)
                    break


if __name__ == '__main__':
    unittest.main(verbosity=1)
