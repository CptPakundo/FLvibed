<#
Lists the tree of a Freelancer UTF file (voice banks DATA\AUDIO\*.utf, models, ...):
one line per node: <path>\t<leaf size or "dir">. With -Extract <dir>, writes every leaf to a file
named after its path (voice banks: <hash>\data = a complete .wav file).

  utf-list.ps1 -File '<bank.utf>' [-Extract '<out dir>']
#>
param(
    [Parameter(Mandatory)] [string] $File,
    [string] $Extract
)
$ErrorActionPreference = 'Stop'
$b = [IO.File]::ReadAllBytes($File)
function U32([int]$o) { [BitConverter]::ToUInt32($b, $o) }
if ([Text.Encoding]::ASCII.GetString($b, 0, 4) -ne 'UTF ') { throw "not a UTF file: $File" }
$nodeOff = U32 8; $strOff = U32 24; $dataOff = U32 36
function Name([int]$o) {
    $s = $strOff + $o; $e = $s
    while ($b[$e] -ne 0) { $e++ }
    [Text.Encoding]::ASCII.GetString($b, $s, $e - $s)
}
$out = [System.Collections.Generic.List[string]]::new()
function Walk([int]$rel, [string]$prefix) {
    while ($true) {
        $n = $nodeOff + $rel
        $name = Name (U32 ($n + 4)); $flags = U32 ($n + 8)
        $path = if ($prefix) { "$prefix\$name" } else { $name }
        if ($flags -band 0x10) {
            $out.Add("$path`tdir")
            Walk (U32 ($n + 16)) $path
        } else {
            $size = U32 ($n + 24); $data = $dataOff + (U32 ($n + 16))
            $out.Add("$path`t$size")
            if ($Extract) {
                $f = Join-Path $Extract ($path.TrimStart('\') -replace '\\', '_')
                [IO.File]::WriteAllBytes($f, $b[$data..($data + $size - 1)])
            }
        }
        $peer = U32 $n
        if ($peer -eq 0) { break }
        $rel = $peer
    }
}
if ($Extract) { New-Item -ItemType Directory -Force $Extract | Out-Null }
Walk 0 ''
$out
