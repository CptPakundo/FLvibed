<#
Freelancer's nickname hash (Common.dll CreateID): voice-bank entries (DATA\AUDIO\*.utf) are named by the
hash of their msg id. Dot-source this file to get `Get-FLHash <string>` (returns a UInt32).
#>
Add-Type -TypeDefinition @'
public static class FlHash {
    static readonly uint[] T = Build();
    static uint[] Build() {
        var t = new uint[256];
        for (uint i = 0; i < 256; i++) {
            uint x = i;
            for (int b = 0; b < 8; b++) x = (x & 1) != 0 ? (x >> 1) ^ (0xA001u << 14) : x >> 1;
            t[i] = x;
        }
        return t;
    }
    public static uint Id(string s) {
        uint h = 0;
        foreach (char ch in s.ToLowerInvariant()) h = (h >> 8) ^ T[(byte)h ^ (byte)ch];
        h = (h >> 24) | ((h >> 8) & 0xFF00) | ((h << 8) & 0xFF0000) | (h << 24);
        return (h >> 2) | 0x80000000;
    }
}
'@ -ErrorAction SilentlyContinue
function Get-FLHash([string]$s) { [FlHash]::Id($s) }
