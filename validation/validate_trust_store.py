#!/usr/bin/env python3
"""Hardware validation of v0.4.0: built-in trust store, verification on by default.

Drives the dongle over USB CDC and checks, against real servers:
  - verification is ON (fresh default, or migrated from 0.3.x settings);
  - with no uploaded CA, the built-in Mozilla store trusts several authorities
    (Let's Encrypt, DigiCert, Sectigo) and still rejects an unknown root, a wrong
    host name and an expired certificate;
  - an uploaded CA (AT$CA=) REPLACES the store (DigiCert refused, ISRG accepted),
    and AT$CA- restores it;
  - AT$CV0 is the explicit insecure opt-out.
Settings are never written (no AT&W): AT$CA- and AT$CV1 restore the state.

SERIAL PROTOCOL: AT commands end with CR only; the PEM upload (AT$CA=) is split
on LF and ends with a line holding only '.'.

Prerequisites: dongle flashed with v0.4.0, WiFi provisioned, Internet access.
Usage: python3 validation/validate_trust_store.py [/dev/ttyACM0]
"""
import sys
import time

import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
BAUD = 115200
HERE = __file__.rsplit("/", 1)[0]

results = []


def rec(name, ok, detail=""):
    results.append((name, ok, detail))
    print(f"  {'✓' if ok else '✗'} {name}" + (f" — {detail}" if detail else ""))


def drain(s, t=1.5):
    end = time.monotonic() + t
    buf = b""
    while time.monotonic() < end:
        n = s.in_waiting
        buf += s.read(n) if n else b""
        time.sleep(0.05)
    return buf.decode("ascii", "replace")


def at(s, line, wait=2.0):
    drain(s, 0.2)
    s.reset_input_buffer()
    s.write((line + "\r").encode())
    s.flush()
    return drain(s, wait)


def wait_for(s, needles, timeout=30.0):
    end = time.monotonic() + timeout
    buf = ""
    while time.monotonic() < end:
        n = s.in_waiting
        if n:
            buf += s.read(n).decode("ascii", "replace")
            for nd in needles:
                if nd in buf:
                    return nd, buf
        time.sleep(0.05)
    return None, buf


def reset_upload(s):
    """Leave any PEM upload mode left over by an interrupted run."""
    s.write(b"\n.\n")
    s.flush()
    time.sleep(0.8)
    drain(s, 1.5)
    s.write(b"\r")
    s.flush()
    drain(s, 0.5)


def send_pem(s, path, settle=8.0):
    drain(s, 0.3)
    s.reset_input_buffer()
    s.write(b"AT$CA=\r")
    s.flush()
    time.sleep(0.4)
    pre = drain(s, 1.0)
    with open(path) as f:
        for ln in f:
            s.write((ln.rstrip("\n") + "\n").encode())
            s.flush()
            time.sleep(0.008)
    s.write(b".\n")
    s.flush()
    return pre + drain(s, settle)


def get(s, host, timeout=30):
    """ATGET https://host/ → CONNECT (verified) or NO CARRIER (refused)."""
    drain(s, 0.4)
    s.reset_input_buffer()
    t0 = time.monotonic()
    s.write(f"ATGEThttps://{host}/\r".encode())
    s.flush()
    nd, _ = wait_for(s, ["CONNECT", "NO CARRIER", "ERROR"], timeout)
    dt = time.monotonic() - t0
    drain(s, 2)
    s.write(b"+++")
    time.sleep(1.2)
    s.write(b"ATH\r")
    time.sleep(0.6)
    drain(s, 0.4)
    return nd or "TIMEOUT", dt


def check_get(s, host, want, why):
    got, dt = get(s, host)
    rec(f"{host} → {want} ({why})", got == want, f"{got}, {dt:.1f} s")


def main():
    print(f"Port {PORT} @ {BAUD}")
    s = serial.Serial(PORT, BAUD, timeout=0.2)
    time.sleep(0.5)
    reset_upload(s)
    at(s, "ATE0")

    print("\n== 0. Firmware identity and defaults ==")
    o = at(s, "ATI", 3.0)
    rec("ATI reports v0.4.0", "v0.4.0" in o, next((l.strip() for l in o.splitlines() if "modem v" in l), "?"))
    o = at(s, "AT$CV?")
    rec("verification ON (default or migrated from 0.3.x)", "1" in o.split("OK")[0], o.strip().splitlines()[0] if o.strip() else "?")
    at(s, "AT$CA-")
    o = at(s, "AT$CA?")
    line = next((l.strip() for l in o.splitlines() if "CA:" in l), "?")
    rec("no uploaded CA → built-in store in use", "built-in store: 150 roots" in line, line)

    print("\n== 1. Built-in store: several authorities ==")
    check_get(s, "badssl.com", "CONNECT", "Let's Encrypt, ISRG Root X1")
    check_get(s, "www.digicert.com", "CONNECT", "DigiCert Global Root G2, RSA")
    check_get(s, "github.com", "CONNECT", "Sectigo E46, ECDSA P-384")

    print("\n== 2. Built-in store: refusals ==")
    check_get(s, "untrusted-root.badssl.com", "NO CARRIER", "unknown root")
    check_get(s, "wrong.host.badssl.com", "NO CARRIER", "wrong host name")
    check_get(s, "expired.badssl.com", "NO CARRIER", "expired; COMODO root is in the store")

    print("\n== 3. Uploaded CA replaces the store ==")
    o = send_pem(s, f"{HERE}/isrg-root-x1.pem")
    rec("AT$CA= ISRG Root X1 stored", "CA stored" in o, o.strip().splitlines()[-1] if o.strip() else "")
    o = at(s, "AT$CA?")
    line = next((l.strip() for l in o.splitlines() if "CA:" in l), "?")
    rec("AT$CA? reports the replacement", "replaces the built-in store" in line, line)
    check_get(s, "badssl.com", "CONNECT", "ISRG = uploaded CA")
    check_get(s, "www.digicert.com", "NO CARRIER", "DigiCert not in the uploaded CA")
    at(s, "AT$CA-")
    check_get(s, "www.digicert.com", "CONNECT", "AT$CA- restores the built-in store")

    print("\n== 4. Explicit insecure opt-out ==")
    rec("AT$CV0", "OK" in at(s, "AT$CV0"))
    check_get(s, "untrusted-root.badssl.com", "CONNECT", "AT$CV0 accepts any certificate")
    rec("AT$CV1 (restore; no CA needed any more)", "OK" in at(s, "AT$CV1"))
    check_get(s, "untrusted-root.badssl.com", "NO CARRIER", "verification back on")

    s.close()
    ok = sum(1 for _, r, _ in results if r)
    print(f"\n== Summary ==\n{ok}/{len(results)} steps passed — port {PORT}, {time.strftime('%Y-%m-%d %H:%M')}\n")
    print("| Step | Result | Detail |\n|---|---|---|")
    for name, r, detail in results:
        print(f"| {name} | {'OK' if r else 'FAIL'} | {detail} |")
    sys.exit(0 if ok == len(results) else 1)


if __name__ == "__main__":
    main()
