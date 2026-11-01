#!/usr/bin/env bash
# Watch a running set / probe (tools\flset.ps1 per instance) and print one line per event - each run's RESULT as it
# comes (progress), each instance's end, then ALL ENDED - the driver's wake-up (the Monitor tool). Polls every 10 s.
# Why (the owner, 31 Oct 2026: "You waited entire minutes while no instance was running. Tighten up those watchers"):
# probe ch2's watcher grepped set.log for "RESULT", which flset writes to <label>.log - all three legs had ended 6 min
# before the driver looked; then a -Runs 2 probe's watcher took run 1's RESULT for the end. An instance has ENDED at:
# flset's "tables regenerated" line in <label>.log (written once after all its runs; -NoTables sets: the last run's
# RESULT with no write for 60 s), a non-empty set.log.err (harness error), or no write to set.log / the run's
# transcript / the instance's live FLController.log for STALE_S seconds (stuck).
# Usage: tools/setwatch.sh <leg> <set> <variant> <instances...>     e.g. tools/setwatch.sh M13 ch3 b 1 2
# Env: STALE_S (default 240), CAP_S (default 2400: the watcher's own limit, printed as TIMEOUT).
set -u
leg="$1"; set="$2"; var="$3"; shift 3
root="$(cd "$(dirname "$0")/.." && pwd)"
dir="$root/research/experience/legs/sets"
stale="${STALE_S:-240}"; cap="${CAP_S:-2400}"
declare -A ended seen tables0
start=$(date +%s)
# (a label flown again appends to the same <label>.log: only lines after the watch began count - 31 Oct 2026, set
# ab13's legs 5-6 on instances whose log already held a "tables regenerated")
# (count: grep -c prints 0 AND exits 1 on no match - "grep -c .. || echo 0" made "0\n0", every later -gt test failed
# and a fresh label never reported: probes sp1 / fx1 / fx2, 31 Oct 2026, four watchers silent to their 30 min cap)
count() { local c; c=$(grep -c -- "$1" "$2" 2>/dev/null); echo "${c:-0}"; }
for i in "$@"; do
    l="$dir/$leg-$set-${var}_i$i.log"
    seen[$i]=$(count 'RESULT' "$l")
    tables0[$i]=$(count 'tables regenerated' "$l")
done
while :; do
    now=$(date +%s)
    left=0
    for i in "$@"; do
        [ -n "${ended[$i]:-}" ] && continue
        s="$dir/$leg-$set-${var}_i$i.set.log"; l="$dir/$leg-$set-${var}_i$i.log"
        # each run's RESULT once, as progress
        if [ -f "$l" ]; then
            n=$(count 'RESULT' "$l"); k="${seen[$i]:-0}"
            if [ "$n" -gt "$k" ]; then
                grep -h 'RESULT' "$l" | tail -n $((n - k)) | while read -r line; do echo "i$i run: $line"; done
                seen[$i]=$n
            fi
        fi
        if [ -f "$l" ] && [ "$(count 'tables regenerated' "$l")" -gt "${tables0[$i]:-0}" ]; then
            echo "i$i ENDED: $(grep -h "^LEG $leg " "$s" 2>/dev/null | tail -1) ($(count 'RESULT' "$l") run(s))"
            ended[$i]=1; continue
        fi
        for e in "$s.err" "$dir/$leg-$set-${var}_i$i.launch.err"; do
            if [ -s "$e" ] && grep -qiv '^warning' "$e"; then
                echo "i$i HARNESS ERROR: $(grep -iv '^warning' "$e" | head -2 | tr '\n' ' ')"
                ended[$i]=1; break
            fi
        done
        [ -n "${ended[$i]:-}" ] && continue
        # (no set.log - a launch without stdout redirected, sp1 / fx1 / fx2: the label's .log is the sign of life)
        if [ -f "$s" ] || [ -f "$l" ]; then
            # sign of life: set.log, the newest run's transcript, the instance's live FLController.log (launchcheck's rule)
            if [ -f "$s" ]; then newest=$(stat -c %Y "$s"); else newest=$(stat -c %Y "$l"); fi
            [ -f "$l" ] && t=$(stat -c %Y "$l") && [ "$t" -gt "$newest" ] && newest=$t
            run=$(ls -td "$dir"/../*"_$leg-$set-${var}_i$i" 2>/dev/null | head -1)
            live="$root/Install/instances/$i/Freelancer HD Edition/EXE/FLController.log"
            for f in "${run:+$run/transcript.txt}" "$live"; do
                [ -n "$f" ] && [ -f "$f" ] && t=$(stat -c %Y "$f") && [ "$t" -gt "$newest" ] && newest=$t
            done
            # (-NoTables: no "tables regenerated" - a RESULT and 60 s of silence end it)
            if [ -f "$l" ] && grep -q 'RESULT' "$l" && [ $((now - newest)) -gt 60 ] && grep -q -- '-NoTables' "$s" 2>/dev/null; then
                echo "i$i ENDED (no tables): $(grep -h 'RESULT' "$l" | tail -1)"; ended[$i]=1; continue
            fi
            if [ $((now - newest)) -gt "$stale" ]; then
                echo "i$i STALE: no log write for $((now - newest)) s (stuck? run tools\\launchcheck.ps1)"
                ended[$i]=1; continue
            fi
        fi
        left=$((left + 1))
    done
    [ "$left" -eq 0 ] && { echo "ALL ENDED ($leg $set $var: $*)"; exit 0; }
    [ $((now - start)) -gt "$cap" ] && { echo "TIMEOUT after $cap s, $left instance(s) still running"; exit 1; }
    sleep 10
done
