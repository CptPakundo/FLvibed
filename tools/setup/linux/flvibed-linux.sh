#!/usr/bin/env bash
# FLvibed on Linux (Wine / Proton): install, uninstall, and the DualSense's waveform haptics.
#
# The game runs under Wine or Proton; FLvibed is a DLL it loads, so installing is what FLvibed-Setup.exe does on Windows:
# the files into the game's EXE folder and one line in EXE/dacom.ini. Run this from the unpacked release zip
# (FLvibed-<version>/), next to its EXE folder.
#
#   ./flvibed-linux.sh install   "<game folder>"   the folder that holds EXE/Freelancer.exe (inside the Wine / Proton prefix)
#   ./flvibed-linux.sh uninstall "<game folder>" [--keep-settings]
#   ./flvibed-linux.sh haptics                     the DualSense's speaker to four channels (Surround 4.0), for the waveform haptics
#
# Waveform haptics (a USB DualSense's grips driven as audio): the pad's speaker must offer four channels, as on Windows
# (there: Sound panel, the pad's speaker, Configure, Quadraphonic). On Linux that is the card profile "Surround 4.0" of
# PipeWire or PulseAudio, which `haptics` sets with pactl. A profile set this way lasts until the pad is unplugged; the
# script prints how to keep it. FLvibed finds the speaker by its name under Wine and says in EXE/FLController.log why
# it fell back to the motors if it could not.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
payload="$here/EXE"
line='FLvibed.dll'

die() { echo "flvibed: $*" >&2; exit 1; }
say() { echo "flvibed: $*"; }

game_exe_dir() {
    local game="${1:-}"
    [ -n "$game" ] || die "name the game folder (the one that holds EXE/Freelancer.exe)"
    local exe
    for exe in "$game/EXE" "$game/exe" "$game"; do
        if [ -f "$exe/Freelancer.exe" ] || [ -f "$exe/freelancer.exe" ]; then
            echo "$exe"; return 0
        fi
    done
    die "no Freelancer.exe in $game/EXE"
}

# dacom.ini keeps its own line endings (the game's file is CRLF)
has_line() { tr -d '\r' < "$1" | grep -qix "[[:space:]]*$line[[:space:]]*"; }

add_line() {
    local ini="$1" tmp
    tmp="$(mktemp)"
    # after the last entry of [Libraries] (the plugins already listed load first), before the next section
    awk -v add="$line" '
        { sub(/\r$/, ""); lines[NR] = $0 }
        END {
            inlib = 0; last = 0; start = 0
            for (i = 1; i <= NR; i++) {
                t = lines[i]; gsub(/^[ \t]+|[ \t]+$/, "", t)
                if (t ~ /^\[/) { if (inlib) break; if (tolower(t) == "[libraries]") { inlib = 1; start = i; last = i } continue }
                if (inlib && t != "" && t !~ /^;/) last = i
            }
            if (!start) { print "NO_LIBRARIES" > "/dev/stderr"; exit 2 }
            for (i = 1; i <= NR; i++) { print lines[i]; if (i == last) print add }
        }' "$ini" > "$tmp" || { rm -f "$tmp"; die "$ini has no [Libraries] section: nothing changed"; }
    put_back "$ini" "$tmp"
}

remove_line() {
    local ini="$1" tmp
    tmp="$(mktemp)"
    awk -v drop="$line" '{ sub(/\r$/, ""); t = $0; gsub(/^[ \t]+|[ \t]+$/, "", t); if (tolower(t) != tolower(drop)) print }' "$ini" > "$tmp"
    put_back "$ini" "$tmp"
}

# the edited text back over the file, in the file's own line endings (CRLF counted as bytes: grep reads a CR differently from shell to shell)
put_back() {
    local ini="$1" tmp="$2" out
    out="$(mktemp)"
    tr -d '\r' < "$tmp" > "$out"
    if [ "$(tr -cd '\r' < "$ini" | wc -c)" -gt 0 ]; then
        awk '{ printf "%s\r\n", $0 }' "$out" > "$tmp"
    else
        cat "$out" > "$tmp"
    fi
    cat "$tmp" > "$ini"
    rm -f "$tmp" "$out"
}

cmd_install() {
    local exe ini
    exe="$(game_exe_dir "${1:-}")"
    ini="$exe/dacom.ini"
    [ -f "$payload/FLvibed.dll" ] || die "run this from the unpacked FLvibed zip (no EXE/FLvibed.dll beside it)"
    [ -f "$ini" ] || die "no $ini"
    mkdir -p "$exe/FLvibed" "$exe/FLSubtitles"
    [ -f "$exe/FLvibed/dacom.ini.before-flvibed" ] || cp "$ini" "$exe/FLvibed/dacom.ini.before-flvibed"
    cp "$payload/FLvibed.dll" "$exe/FLvibed.dll"
    cp "$payload/FLSubtitles/"* "$exe/FLSubtitles/"
    cp "$payload/FLvibed/"* "$exe/FLvibed/"
    cp "$here/$(basename "${BASH_SOURCE[0]}")" "$exe/FLvibed/flvibed-linux.sh" 2>/dev/null || true
    if has_line "$ini"; then say "dacom.ini already lists $line"; else add_line "$ini"; say "dacom.ini: $line listed"; fi
    say "installed into $exe (your FLvibed settings, if any, stay)"
    if command -v pactl >/dev/null 2>&1 && dualsense_card >/dev/null; then cmd_haptics || true; fi
}

cmd_uninstall() {
    local exe ini keep=0
    exe="$(game_exe_dir "${1:-}")"
    [ "${2:-}" = "--keep-settings" ] && keep=1
    ini="$exe/dacom.ini"
    [ -f "$ini" ] && has_line "$ini" && { remove_line "$ini"; say "dacom.ini: $line removed"; }
    rm -f "$exe/FLvibed.dll" "$exe/FLController.log" "$exe/FLSubtitles.log" "$exe/FLvibed_npc_stalls.log"
    rm -rf "$exe/FLSubtitles" "$exe/FLvibed"
    if [ "$keep" = 0 ]; then
        rm -f "$exe/FLvibed.ini" "$exe/FLController.ini" "$exe/FLSubtitles.ini" "$exe"/*.before-0.2.0
        say "uninstalled from $exe (settings removed)"
    else
        say "uninstalled from $exe (settings kept)"
    fi
}

# the DualSense's card as PipeWire / PulseAudio name it (alsa_card.usb-Sony_Interactive_Entertainment_DualSense_..., the Edge too)
dualsense_card() {
    command -v pactl >/dev/null 2>&1 || return 1
    pactl list cards short 2>/dev/null | awk '{print $2}' | grep -i -E 'dualsense|sony_interactive' | head -n 1 | grep .
}

cmd_haptics() {
    command -v pactl >/dev/null 2>&1 || die "pactl not found (PipeWire's pipewire-pulse or PulseAudio provides it): install it, or set the DualSense's profile to Surround 4.0 in your sound settings"
    local card profile
    card="$(dualsense_card)" || die "no DualSense sound card: plug the pad in by USB (Bluetooth has no waveform)"
    for profile in "output:analog-surround-40+input:mono-fallback" "output:analog-surround-40+input:analog-mono" "output:analog-surround-40"; do
        if pactl set-card-profile "$card" "$profile" 2>/dev/null; then
            say "the DualSense's speaker is four channels now ($profile on $card): the waveform haptics can start"
            say "to keep it after a replug: pick 'Analog Surround 4.0 Output' for the DualSense in your sound settings, which most desktops remember"
            return 0
        fi
    done
    die "the card $card took none of the Surround 4.0 profiles: see 'pactl list cards' for its profiles and set one with four channels"
}

case "${1:-}" in
    install) shift; cmd_install "$@" ;;
    uninstall) shift; cmd_uninstall "$@" ;;
    haptics) cmd_haptics ;;
    *) sed -n '2,12p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 1 ;;
esac
