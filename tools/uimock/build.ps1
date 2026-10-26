# Builds the UI mock-up page from its parts into the mock-up folder (beside the images) and syntax-checks the script.
#   tools\uimock\build.ps1 [-Out <folder>]   default: research\evidence\controller_ui\mockup
param([string]$Out = '')
$here = $PSScriptRoot
$root = Split-Path (Split-Path $here -Parent) -Parent
if (-not $Out) { $Out = Join-Path $root 'research\evidence\controller_ui\mockup' }
New-Item -ItemType Directory -Force $Out | Out-Null
$parts = @('part1_head.html'); $script = @('part2_data.js', 'part2b_doc.js', 'part3a_render.js', 'part3c_doc.js', 'part3b_editor.js')
$html = (Get-Content (Join-Path $here 'part1_head.html') -Raw) + "<script>`n" + (($script | ForEach-Object { Get-Content (Join-Path $here $_) -Raw }) -join "`n") + "`n</script>`n"
$index = Join-Path $Out 'index.html'
[IO.File]::WriteAllText($index, $html, [Text.UTF8Encoding]::new($false))
$js = Join-Path $env:TEMP 'uimock_check.js'
[IO.File]::WriteAllText($js, (($script | ForEach-Object { Get-Content (Join-Path $here $_) -Raw }) -join "`n"), [Text.UTF8Encoding]::new($false))
& node --check $js; if ($LASTEXITCODE -ne 0) { throw 'syntax error in the mock-up script' }
Remove-Item $js -ErrorAction SilentlyContinue
"built $index ($([IO.File]::ReadAllBytes($index).Length) bytes); images expected in $Out\img"
