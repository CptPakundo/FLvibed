param([string]$From, [string]$To, [string]$File = "Freelancer")
$f = Join-Path (Split-Path $PSScriptRoot -Parent) "research\disasm\$File.ann.asm"
if (-not (Test-Path $f)) { $f = Join-Path (Split-Path $PSScriptRoot -Parent) "research\disasm\$File.asm" } # Common, Content: plain dumps
$a0 = [Convert]::ToUInt32(($From -replace '^0x',''),16); $a1 = [Convert]::ToUInt32(($To -replace '^0x',''),16)
$reader = [IO.StreamReader]::new($f)
$printing = $false
while (($l = $reader.ReadLine()) -ne $null) {
  if ($l.Length -gt 11 -and $l[10] -eq ':' -and $l.StartsWith('  ')) {
    $hex = $l.Substring(2,8)
    $ok = $true; foreach ($c in $hex.ToCharArray()) { if (-not ([Uri]::IsHexDigit($c))) { $ok = $false; break } }
    if ($ok) { $a = [Convert]::ToUInt32($hex,16); if ($a -ge $a0 -and $a -le $a1) { $printing = $true } elseif ($a -gt $a1) { break } else { $printing = $false } }
  }
  if ($printing) { $l }
}
$reader.Close()
