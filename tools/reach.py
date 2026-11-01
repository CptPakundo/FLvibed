"""reach.py - the smallest effect a set can detect on one segment, from the history (docs\\TESTING.md "The test loop" 4:
about 2.5 x sigma x sqrt(2 / cap); the Q4 retro, 30 Oct 2026: three M13 sets of 24 legs ended without a verdict because
nobody computed this first). Read-only: imports tools\\splits.py's history (the last RECENT full passes' segment spread),
writes nothing. Used by the command guard's no-reach rule (tools\\claude\\hooks\\guard.js; audit 31 Oct 2026 D1).

Usage: reach.py <checkpoint, e.g. M13.bsdown | leg, e.g. M13> <runs per arm (the cap)>
Prints one line:  REACH <cp> sd <s> n <recent runs> cap <n> detect <d>
or                REACH <cp> NODATA <why>   (fewer than 3 recent full passes, an unknown checkpoint)
Exit 0 always (advice: the guard decides; NODATA lets the launch pass with the stated reach)."""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import splits  # noqa: E402  (its main() runs only as a script)

K = 2.5  # TESTING.md's factor: ~alpha 0.05 / beta 0.2 two-arm sequential test at the cap


def detectable(sd, cap):
    return K * max(sd, splits.SPRT_SIGMA_MIN) * math.sqrt(2.0 / cap)


def main(a):
    if len(a) != 2 or not a[1].isdigit() or int(a[1]) < 1:
        print(__doc__)
        return
    target, cap = a[0], int(a[1])
    legs = splits.checkpoints()
    leg = target.split('.')[0]
    if leg not in legs:
        print(f'REACH {target} NODATA unknown leg')
        return
    st = splits.ours_stats(legs)[leg]
    if '.' in target:
        ks = [k for k, cp in enumerate(legs[leg]) if cp['cp'] == target]
        if not ks:
            print(f'REACH {target} NODATA unknown checkpoint')
            return
        sd = st['segsd'][ks[0]]
    else:
        sd = st['sd']
    if sd is None:
        print(f'REACH {target} NODATA fewer than 3 recent full passes')
        return
    print(f'REACH {target} sd {sd:.1f} n {st["nrecent"]} cap {cap} detect {detectable(sd, cap):.1f}')


def selftest():
    ok = abs(detectable(26.6, 12) - 27.15) < 0.1 and abs(detectable(0.2, 12) - 1.02) < 0.05  # Q4s3; the floor
    print('selftest:', 'OK' if ok else 'FAIL')
    return ok


if __name__ == '__main__':
    if sys.argv[1:] == ['selftest']:
        sys.exit(0 if selftest() else 1)
    main(sys.argv[1:])
