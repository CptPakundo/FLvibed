# Before a game is spent on a test script: every harness call in tests\*.ps1 must resolve. Reads each script's AST:
# a command named <Verb>-FL... must be a function of the script itself or of a file it dot-sources (flctl.ps1 brings its flctl.*.ps1 parts),
# and each named parameter it passes must exist on that function. Written after P11c lost seven launches and P11e one to their own scripts.
# Usage (PowerShell tool): tools\test_lint.ps1 [-Files a.ps1,b.ps1]    exit 1 = a call that would die in the game's first minute.
param([string[]]$Files = @())
$root = Split-Path $PSScriptRoot -Parent
$harness = Join-Path $root 'tools\harness'
if (-not $Files) { $Files = @(Get-ChildItem (Join-Path $root 'tests') -Filter *.ps1 | ForEach-Object FullName) }

function Parse([string]$f) {
    $tokens = $null; $errors = $null
    $ast = [Management.Automation.Language.Parser]::ParseFile($f, [ref]$tokens, [ref]$errors)
    [pscustomobject]@{ Ast = $ast; Errors = $errors }
}
# name -> parameter names of every function a file defines, and the files it dot-sources by a literal name
function Functions($ast) {
    $out = @{}
    foreach ($fn in $ast.FindAll({ param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] }, $true)) {
        $ps = if ($fn.Parameters) { $fn.Parameters } elseif ($fn.Body.ParamBlock) { $fn.Body.ParamBlock.Parameters } else { @() }
        $out[$fn.Name] = @($ps | ForEach-Object { $_.Name.VariablePath.UserPath })
    }
    $out
}
$lib = @{}
$cache = @{}
function Library([string]$name) {
    if ($cache.ContainsKey($name)) { return $cache[$name] }
    $all = @{}
    $files = @(Get-ChildItem $harness -Filter "$($name -replace '\.ps1$', '')*.ps1" | ForEach-Object FullName)
    if ($name -eq 'flctl.ps1') { $files += @((Join-Path $root 'tools\fldata.ps1'), (Join-Path $root 'tools\flhash.ps1'), (Join-Path $harness 'flroute.ps1')) } # (what it dot-sources)
    foreach ($f in $files) {
        $p = Parse $f
        $fns = Functions $p.Ast
        foreach ($k in $fns.Keys) { $all[$k] = $fns[$k] }
    }
    $cache[$name] = $all
    $all
}

$bad = [Collections.Generic.List[string]]::new(); $calls = 0
foreach ($f in $Files) {
    $p = Parse $f
    $leaf = Split-Path $f -Leaf
    foreach ($e in $p.Errors) { $bad.Add("${leaf}:$($e.Extent.StartLineNumber): $($e.Message)") }
    $known = Functions $p.Ast
    $text = Get-Content -Raw $f
    foreach ($m in [regex]::Matches($text, "(?m)^\s*\.\s.*'tools\\harness\\(\w+\.ps1)'")) {
        $l = Library $m.Groups[1].Value
        foreach ($k in $l.Keys) { $known[$k] = $l[$k] }
    }
    foreach ($m in [regex]::Matches($text, "(?m)^\s*\.\s.*'(tools\\(?!harness\\)[\w\\]+\.ps1)'")) {
        $l = Functions (Parse (Join-Path $root $m.Groups[1].Value)).Ast
        foreach ($k in $l.Keys) { $known[$k] = $l[$k] }
    }
    foreach ($c in $p.Ast.FindAll({ param($n) $n -is [Management.Automation.Language.CommandAst] }, $true)) {
        $name = $c.GetCommandName()
        if (-not $name -or $name -notmatch '^[A-Z][a-z]+-FL') { continue }
        $calls++
        $line = $c.Extent.StartLineNumber
        if (-not $known.ContainsKey($name)) { $bad.Add("${leaf}:${line}: $name is not defined by the script or what it dot-sources"); continue }
        foreach ($el in $c.CommandElements) {
            if ($el -isnot [Management.Automation.Language.CommandParameterAst]) { continue }
            $hit = @($known[$name] | Where-Object { $_ -like "$($el.ParameterName)*" })
            if (-not $hit) { $bad.Add("${leaf}:${line}: $name has no parameter -$($el.ParameterName)") }
        }
    }
}
if ($bad.Count) { "test lint: $($bad.Count) problem(s) in $($Files.Count) scripts ($calls harness calls):"; $bad | ForEach-Object { "  $_" }; exit 1 }
"test lint: OK - $($Files.Count) scripts, $calls harness calls resolve"
exit 0
