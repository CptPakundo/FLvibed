# Creates free-roam test saves (no story mission, no timers/fail states) for controller testing.
# HD Edition saves are plain-text INI. The template is any existing single-player save; every
# mission-specific block between [MissionState] and [StoryInfo] is replaced with a clean state and
# the story is set to No_Mission (Content.dll's post-story free-roam state).
#
#   make-testsaves.ps1 [-Template <save.fl>]
# Writes (never overwrites other saves); they sort right below AUTO SAVE:
#   SaveC702.fl  "CTL free roam - space Manhattan"
#   SaveC701.fl  "CTL free roam - docked Manhattan"
#   SaveC703.fl  "CTL free roam - space Manhattan, gates open" (as SaveC702 without the template's `locked_gate`
#                lines: the story locks jump gates and holes by its progress, and the gate answers a dock request
#                with `info_access_denied`)
#   SaveC704.fl  "CTL free roam - space Manhattan, or_elite" (from the campaign set's post-story save, decoded:
#                a second ship for benchmark scenarios)
# -Only SaveC703.fl writes just the named ones.
# Freelancer reads the save list at startup: restart the game after creating them the first time.
# Load one by name: Load-FLSave SaveC702 (flctl.ps1, native). For enemies use the HDE console
# (Send-FLConsole "rep l H" makes the Liberty police hostile; "godmode" takes effect on the next load).
param(
    [string]$Template = '',
    [int]$Cash = 500000,
    [string[]]$Only
)

# The saves go to the test install's own save folder (tools\fldata.ps1), never a personal install's.
. (Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'tools\fldata.ps1')
$saveDir = Get-FLSaveDir
if (-not $Template) { $Template = Join-Path $saveDir 'AutoSave.fl' }
$lines = Get-Content -LiteralPath $Template

function Hex16([string]$s) { ($s.ToCharArray() | ForEach-Object { '{0:x4}' -f [int]$_ }) -join '' }

function Build([string]$description, [scriptblock]$placement, [bool]$gatesOpen = $false) {
    $out = New-Object System.Collections.Generic.List[string]
    # skip: 'none' | 'mission' (from [MissionState] up to [StoryInfo]) | 'section' (rest of one section)
    $skip = 'none'
    $inPlayer = $false
    foreach ($l in $lines) {
        if ($l -match '^\[(.+)\]') {
            $section = $Matches[1]
            $inPlayer = $section -eq 'Player'
            if ($skip -eq 'section') { $skip = 'none' }
            if ($section -eq 'MissionState') {
                $skip = 'mission'
                # Free roam: no clamps, normal traffic and trade lane attacks.
                $out.AddRange([string[]]@('[MissionState]', 'mission_accepted = 0', 'att_clamp = false',
                    'tradelane_attacks = true', 'scan_clamp = false', 'gcs_clamp = false', 'hostile_clamp = false',
                    'random_pop = true', '', '[BStoryMissionDone]', ''))
                continue
            }
            if ($section -eq 'StoryInfo') {
                $out.AddRange([string[]]@('[StoryInfo]', 'ship_bought = true', 'Mission = No_Mission',
                    'MissionNum = 0', 'delta_worth = -1.000000', 'debug = 0', ''))
                $skip = 'section' # drop the template's story keys
                continue
            }
        }
        if ($skip -ne 'none') { continue }
        # Missions can revoke docking / trade lane use (Act_PlayerCanDock / Act_PlayerCanTradelane);
        # the flags persist in [mPlayer].
        if ($l -match '^can_dock\s*=') { $out.Add('can_dock = 1'); continue }
        if ($l -match '^can_tl\s*=') { $out.Add('can_tl = 1'); continue }
        if ($gatesOpen -and $l -match '^locked_gate\s*=') { continue }
        if ($inPlayer) {
            if ($l -match '^(descrip_strid|description)\s*=') { $out.Add("description = $(Hex16 $description)"); continue }
            if ($l -match '^money\s*=') { $out.Add("money = $Cash"); continue }
            if ($l -match '^tstamp\s*=') {
                # Save lists sort newest first: a timestamp far in the future keeps the test saves at the top
                # for a person browsing the list (FILETIME high,low). Tests load them by file name (Load-FLSave).
                $ft = [DateTime]::new(2099, 1, 1, 0, 0, $tstampSeconds, [DateTimeKind]::Utc).ToFileTimeUtc()
                $out.Add(('tstamp = {0},{1}' -f ($ft -shr 32), ($ft -band 0xFFFFFFFFL))); continue
            }
            if ($l -match '^(base|location|pos|rotate)\s*=') { continue } # replaced by the placement below
            if ($l -match '^system\s*=') { $out.Add($l); $out.AddRange([string[]](& $placement)); continue }
        }
        $out.Add($l)
    }
    $out
}

$saves = @(
    @{ File = 'SaveC702.fl'; Desc = 'CTL free roam - space Manhattan'; Seconds = 2
       # Fort Bush end of the Manhattan - Fort Bush lane (ring 141): constant traffic and patrols, ~3 km
       # from Fort Bush and the nearby stations, ~50 km from any planet (planets kill even in godmode)
       Place = { 'pos = 8000, 800, -50500', 'rotate = 0, 0, 0' } },
    @{ File = 'SaveC701.fl'; Desc = 'CTL free roam - docked Manhattan'; Seconds = 1; Place = { 'base = Li01_01_Base' } },
    @{ File = 'SaveC703.fl'; Desc = 'CTL free roam - space Manhattan, gates open'; Seconds = 3; GatesOpen = $true
       Place = { 'pos = 8000, 800, -50500', 'rotate = 0, 0, 0' } },
    # A second player ship for benchmark scenarios: the campaign set's post-story save (or_elite, rank 21;
    # ..\..\docs\missions\SAVES.md), decoded by tools\flsave.js, placed like SaveC702 (in space: no launch needed).
    @{ File = 'SaveC704.fl'; Desc = 'CTL free roam - space Manhattan, or_elite'; Seconds = 4
       Template = Join-Path (Split-Path (Split-Path $PSScriptRoot)) 'research\saves\bc46-all-missions\decoded\Save0e2b.fl.ini'
       Place = { 'pos = 8000, 800, -50500', 'rotate = 0, 0, 0' } }
)
foreach ($s in $saves) {
    if ($Only -and $s.File -notin $Only) { continue }
    if ($s.Template) {
        if (-not (Test-Path -LiteralPath $s.Template)) { Write-Warning "$($s.File): template $($s.Template) missing"; continue }
        $lines = Get-Content -LiteralPath $s.Template
    } else { $lines = Get-Content -LiteralPath $Template }
    $path = Join-Path $saveDir $s.File
    $tstampSeconds = $s.Seconds
    $content = Build $s.Desc $s.Place ([bool]$s.GatesOpen)
    Set-Content -LiteralPath $path -Value $content -Encoding ascii
    "{0}  {1}  ({2} lines)" -f $s.File, $s.Desc, $content.Count
}
