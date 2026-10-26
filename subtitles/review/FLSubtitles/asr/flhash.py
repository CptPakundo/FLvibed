"""Freelancer's nickname hash (Common.dll CreateID); voice-bank entries are named by the hash of their msg id."""

_T = []
for _i in range(256):
    _x = _i
    for _ in range(8):
        _x = (_x >> 1) ^ (0xA001 << 14) if _x & 1 else _x >> 1
    _T.append(_x)


def create_id(s: str) -> int:
    h = 0
    for c in s.lower().encode("latin-1"):
        h = (h >> 8) ^ _T[(h ^ c) & 0xFF]
    h = ((h >> 24) | ((h >> 8) & 0xFF00) | ((h << 8) & 0xFF0000) | (h << 24)) & 0xFFFFFFFF
    return (h >> 2) | 0x80000000
