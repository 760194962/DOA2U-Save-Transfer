#!/usr/bin/env python3
"""
DOA2U Save Transfer (Python version, standard library only)

Transfer a Dead or Alive Ultimate (Xbox, TitleID 54430006) ups.dat between
consoles / xemu / Xbox 360 backward compatibility.

  python doau_transfer.py verify  ups.dat --mac 00:50:F2:AA:BB:CC [--hdkey TARGET_HDKEY]
  python doau_transfer.py convert src_ups.dat out_ups.dat \
         --src-mac 00:50:F2:AA:BB:CC --dst-mac 00:25:AE:DD:EE:FF --hdkey TARGET_HDKEY

  python doau_transfer.py foldername "Xbox360Jeremy" --zwsp
  python doau_transfer.py foldername path/to/SaveMeta.xbx      # also checks the folder name

Searching for an unknown MAC is far too slow in Python; use the Windows exe or
the C command line tool (see README).
"""
import argparse, hmac, hashlib, struct, sys

UPS_SIZE = 74056
MAC_OFFSET = 0x18 + 0xB1E9
NAME_OFFSET = 0x18 + 0xB1A9
MAGIC = b"Lightning Offering Guy\x00"
XBOX_CERT_KEY = bytes.fromhex("5C0733AE0401F7E8BA7993FDCD2F1FE0")
DOAU_TITLE_KEY = bytes.fromhex("42CBF532BF1FD14739D0F7F66DF8B7B1")

# ---------------------------------------------------------------- signature
def _h(k, d): return hmac.new(k, d, hashlib.sha1).digest()
SIG_KEY = _h(XBOX_CERT_KEY, DOAU_TITLE_KEY)[:16]
def sign(data, hdkey):             # non-roamable XCalculateSignature
    return _h(hdkey, _h(SIG_KEY, data))

# ---------------------------------------------------------------- MT19937
class MT:
    def __init__(self, key):
        mt = [0] * 624
        mt[0] = 19650218
        for i in range(1, 624):
            mt[i] = (1812433253 * (mt[i-1] ^ (mt[i-1] >> 30)) + i) & 0xFFFFFFFF
        i, j = 1, 0
        for _ in range(max(624, len(key))):
            mt[i] = ((mt[i] ^ ((mt[i-1] ^ (mt[i-1] >> 30)) * 1664525)) + key[j] + j) & 0xFFFFFFFF
            i += 1; j += 1
            if i >= 624: mt[0] = mt[623]; i = 1
            if j >= len(key): j = 0
        for _ in range(623):
            mt[i] = ((mt[i] ^ ((mt[i-1] ^ (mt[i-1] >> 30)) * 1566083941)) - i) & 0xFFFFFFFF
            i += 1
            if i >= 624: mt[0] = mt[623]; i = 1
        mt[0] = 0x80000000
        self.mt, self.i = mt, 624

    def _twist(self):
        mt = self.mt
        for k in range(624):
            y = (mt[k] & 0x80000000) | (mt[(k + 1) % 624] & 0x7FFFFFFF)
            mt[k] = mt[(k + 397) % 624] ^ (y >> 1) ^ (0x9908B0DF if y & 1 else 0)
        self.i = 0

    def next(self):
        if self.i >= 624: self._twist()
        y = self.mt[self.i]; self.i += 1
        y ^= y >> 11
        y ^= (y << 7) & 0x9D2C5680
        y ^= (y << 15) & 0xEFC60000
        y ^= y >> 18
        return y

# ---------------------------------------------------------------- Blowfish
def _pi_tables():
    # Blowfish initial values = fractional hex digits of pi (1042 words).
    # Computed with a spigot so the script stays self-contained.
    # pi with enough bits via Machin's formula, integer arithmetic only
    prec = 1042 * 32 + 64
    def arctan_inv(v):
        s = term = (1 << prec) // v
        v2, i, sign_ = v * v, 3, -1
        while term:
            term //= v2
            s += sign_ * (term // i)
            i += 2; sign_ = -sign_
        return s
    pi = 16 * arctan_inv(5) - 4 * arctan_inv(239)     # pi << prec
    frac = pi - (3 << prec)
    words = []
    for _ in range(1042):
        frac <<= 32
        words.append(frac >> prec)
        frac &= (1 << prec) - 1
    return words[:18], [words[18 + 256 * s: 18 + 256 * (s + 1)] for s in range(4)]
BF_P, BF_S = _pi_tables()
assert BF_P[0] == 0x243F6A88 and BF_S[0][0] == 0xD1310BA6

class Blowfish:
    def __init__(self, key):
        self.P = list(BF_P); self.S = [list(s) for s in BF_S]
        j = 0
        for i in range(18):
            d = 0
            for _ in range(4):
                d = (d << 8) | key[j]; j = (j + 1) % len(key)
            self.P[i] ^= d
        l = r = 0
        for i in range(0, 18, 2):
            l, r = self.enc(l, r); self.P[i], self.P[i+1] = l, r
        for s in range(4):
            for i in range(0, 256, 2):
                l, r = self.enc(l, r); self.S[s][i], self.S[s][i+1] = l, r

    def _f(self, x):
        S = self.S
        return ((((S[0][x >> 24] + S[1][(x >> 16) & 255]) & 0xFFFFFFFF) ^ S[2][(x >> 8) & 255]) + S[3][x & 255]) & 0xFFFFFFFF

    def enc(self, l, r):
        P = self.P
        for i in range(16):
            l ^= P[i]; r ^= self._f(l); l, r = r, l
        l, r = r, l
        return l ^ P[17], r ^ P[16]

    def dec(self, l, r):
        P = self.P
        for i in range(17, 1, -1):
            l ^= P[i]; r ^= self._f(l); l, r = r, l
        l, r = r, l
        return l ^ P[0], r ^ P[1]

# ---------------------------------------------------------------- ups.dat
def _setup(buf, mac):
    seed = struct.unpack_from("<I", buf, 20)[0]
    key = [seed, struct.unpack("<I", mac[:4])[0], mac[4] | (mac[5] << 8)]
    m = MT(key)
    kw = struct.pack("<14I", *[m.next() for _ in range(14)])
    return m, Blowfish(kw)

def decrypt(buf, mac):
    m, bf = _setup(buf, mac)
    w = list(struct.unpack_from("<%dI" % ((UPS_SIZE - 24) // 4), buf, 24))
    w = [x ^ m.next() for x in w]
    for i in range(0, len(w), 2):
        w[i], w[i+1] = bf.dec(w[i], w[i+1])
    return bytes(buf[:24]) + struct.pack("<%dI" % len(w), *w)

def encrypt(buf, mac):
    m, bf = _setup(buf, mac)
    w = list(struct.unpack_from("<%dI" % ((UPS_SIZE - 24) // 4), buf, 24))
    for i in range(0, len(w), 2):
        w[i], w[i+1] = bf.enc(w[i], w[i+1])
    w = [x ^ m.next() for x in w]
    return bytes(buf[:24]) + struct.pack("<%dI" % len(w), *w)

def plain_ok(p): return p[24] == 0 and p[-23:] == MAGIC

def profile_info(p):
    raw = p[NAME_OFFSET:NAME_OFFSET + 32]
    name = raw.decode("utf-16-le", "replace").split("\x00")[0]
    return name, p[MAC_OFFSET:MAC_OFFSET + 6]

# ------------------------------------------------------- save folder name
# XAPI (XCreateSaveGame) derives the 12-hex-digit save folder name from the
# save name. Read from DOA2.xbe (XDK 5849, function at 0x2b9d02):
#     h = 0
#     for each UTF-16 code unit c of the name:  h = (h * 0x10000 + c) mod (2**48 - 59)
#     folder = "%012X" % h
# The name is the text after "Name=" in SaveMeta.xbx, exactly as stored: DOA
# profile names end with U+200B (zero-width space), which is part of the hash.
FOLDER_MOD = (1 << 48) - 59

def save_folder_name(name):
    h = 0
    b = name.encode("utf-16-le")
    for i in range(0, len(b), 2):
        h = (h * 0x10000 + int.from_bytes(b[i:i + 2], "little")) % FOLDER_MOD
    return "%012X" % h

def read_savemeta_name(path):
    text = open(path, "rb").read().decode("utf-16")          # BOM FFFE
    for line in text.split("\r\n"):
        if line.startswith("Name="):
            return line[5:]
    sys.exit("✗ no Name= line in %s" % path)

# ---------------------------------------------------------------- CLI
def parse_hex(s, n, what):
    h = "".join(c for c in s if c not in ":- ")
    try:
        b = bytes.fromhex(h)
    except ValueError:
        b = b""
    if len(b) != n:
        sys.exit("✗ %s must be %d hex digits" % (what, 2 * n))
    return b

def fmt(mac): return ":".join("%02X" % x for x in mac)

def load(path):
    b = open(path, "rb").read()
    if len(b) != UPS_SIZE:
        sys.exit("✗ %s is %d bytes, ups.dat should be %d" % (path, len(b), UPS_SIZE))
    return b

def cmd_verify(a):
    b = load(a.file)
    if a.hdkey:
        hd = parse_hex(a.hdkey, 16, "HD key")
        print("signature with HD key:", "OK" if sign(b[20:], hd) == b[:20] else "MISMATCH")
    if a.mac:
        p = decrypt(b, parse_hex(a.mac, 6, "MAC"))
        if plain_ok(p):
            name, emb = profile_info(p)
            print("MAC: OK   profile: %s   embedded MAC: %s" % (name, fmt(emb)))
        else:
            print("MAC: WRONG (cannot decrypt with this MAC)")

def cmd_convert(a):
    src = load(a.src)
    smac = parse_hex(a.src_mac, 6, "source MAC")
    dmac = parse_hex(a.dst_mac, 6, "target MAC")
    hd = parse_hex(a.hdkey, 16, "HD key")
    p = bytearray(decrypt(src, smac))
    if not plain_ok(p):
        sys.exit("✗ source MAC cannot decrypt the source save")
    name, emb = profile_info(p)
    p[MAC_OFFSET:MAC_OFFSET + 6] = dmac
    out = bytearray(encrypt(p, dmac))
    out[:20] = sign(bytes(out[20:]), hd)
    open(a.out, "wb").write(out)
    print("✓ wrote %s  (profile %s, embedded MAC %s -> %s)" % (a.out, name, fmt(emb), fmt(dmac)))
    print("  Copy the WHOLE save folder (with SaveMeta.xbx / SaveImage.xbx) to UDATA/54430006/, not just ups.dat.")

def cmd_foldername(a):
    import os
    if os.path.isfile(a.name):
        name = read_savemeta_name(a.name)
        calc = save_folder_name(name)
        print("save name: %r" % name)
        print("calculated folder: %s" % calc)
        actual = os.path.basename(os.path.dirname(os.path.abspath(a.name)))
        print("actual folder:     %s  %s" % (actual, "✓ match" if actual.upper() == calc else "✗ different"))
    else:
        name = a.name + ("\u200b" if a.zwsp else "")
        print(save_folder_name(name))

def main():
    ap = argparse.ArgumentParser(description="Dead or Alive Ultimate ups.dat transfer tool")
    sp = ap.add_subparsers(dest="cmd", required=True)
    v = sp.add_parser("verify", help="check a ups.dat against a MAC and/or HD key")
    v.add_argument("file"); v.add_argument("--mac"); v.add_argument("--hdkey")
    v.set_defaults(fn=cmd_verify)
    c = sp.add_parser("convert", help="re-encrypt and re-sign a ups.dat for another console")
    c.add_argument("src"); c.add_argument("out")
    c.add_argument("--src-mac", required=True); c.add_argument("--dst-mac", required=True)
    c.add_argument("--hdkey", required=True, help="target console XboxHDKey")
    c.set_defaults(fn=cmd_convert)
    f = sp.add_parser("foldername", help="calculate the save folder name from a save name or a SaveMeta.xbx")
    f.add_argument("name", help="save name, or path to a SaveMeta.xbx")
    f.add_argument("--zwsp", action="store_true", help="append U+200B (DOA profile names end with it)")
    f.set_defaults(fn=cmd_foldername)
    a = ap.parse_args(); a.fn(a)

if __name__ == "__main__":
    main()
