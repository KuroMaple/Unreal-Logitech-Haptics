# Applies the group color scheme (docs/ring-profile.md) to every UnrealMxBridge action on the
# Unreal Engine Actions Ring profile in Logi Options+.
#
# - Icons picked from the Options+ icon marketplace keep their image; only the tint changes.
# - Plugin-glyph icons are re-rendered from the plugin's glyphs as monochrome SVGs, so the tint
#   always applies (fixes icons captured from older two-color images).
# - Background and text settings of existing icons are kept.
#
# Stops and restarts Logi Plugin Service so it doesn't overwrite the files while writing.
# Usage: powershell -ExecutionPolicy Bypass -File scripts\apply-ring-colors.ps1

$ErrorActionPreference = "Stop"
$repo = Split-Path $PSScriptRoot -Parent
$src = Join-Path $repo "LogiPlugin\src\UnrealMxBridgePlugin"

$groupColors = @{
    Play      = "#3DDC84"; Stop = "#D93025"; Build = "#F9AB00"; Transform = "#1A73E8"
    Viewport  = "#B78CFF"; Navigate = "#00897B"
    NudgeX    = "#E5484D"; NudgeY = "#3DDC84"; NudgeZ = "#1A73E8"   # gizmo axis colors
}
$classGroup = @{
    PiePlayCommand = "Play"; PieSimulateCommand = "Play"; PieStopCommand = "Stop"
    LiveCodingCommand = "Build"; CompileBlueprintsCommand = "Build"; SaveAllCommand = "Build"; BuildLightingCommand = "Build"
    TranslateModeCommand = "Transform"; RotateModeCommand = "Transform"; ScaleModeCommand = "Transform"
    ToggleSpaceCommand = "Transform"; ToggleGridSnapCommand = "Transform"; SnapToFloorCommand = "Transform"
    NudgeXAdjustment = "NudgeX"; NudgeYAdjustment = "NudgeY"; NudgeZAdjustment = "NudgeZ"
    FocusSelectedCommand = "Viewport"; IsolateSelectedCommand = "Viewport"; Bookmark1Command = "Viewport"
    GameViewCommand = "Viewport"; LitViewCommand = "Viewport"; UnlitViewCommand = "Viewport"; WireframeViewCommand = "Viewport"
    ContentBrowserCommand = "Navigate"; SyncBrowserCommand = "Navigate"; OpenAssetCommand = "Navigate"; OutputLogCommand = "Navigate"
}

# Glyph SVG bodies and class -> (glyph, display name), parsed from the plugin source.
$glyphs = @{}
foreach ($m in [regex]::Matches((Get-Content "$src\Icons\IconRenderer.cs" -Raw), 'public const string (\w+)\s*=\s*"([^"]*)";')) {
    $glyphs[$m.Groups[1].Value] = $m.Groups[2].Value
}
$actions = @{}
foreach ($m in [regex]::Matches((Get-Content "$src\Commands\UnrealCommands.cs" -Raw), 'class (\w+)\(\) : UnrealCommand\(\s*"[^"]+", "([^"]+)", "[^"]+", "[^"]+", Glyphs\.(\w+)')) {
    $actions[$m.Groups[1].Value] = @{ Name = $m.Groups[2].Value; Glyph = $m.Groups[3].Value }
}
foreach ($m in [regex]::Matches((Get-Content "$src\Adjustments\NudgeAdjustment.cs" -Raw), 'class (\w+)\(\) : NudgeAdjustment\("[^"]+", "(\w)", Glyphs\.(\w+)\)')) {
    $actions[$m.Groups[1].Value] = @{ Name = "Nudge " + $m.Groups[2].Value; Glyph = $m.Groups[3].Value }
}

function Get-GlyphSvg([string]$glyphName) {
    if (-not $glyphs.ContainsKey($glyphName)) { throw "Glyph '$glyphName' not found in IconRenderer.cs" }
    $body = $glyphs[$glyphName].Replace("'C'", "'#FFFFFF'")
    "<svg xmlns='http://www.w3.org/2000/svg' width='80' height='80' viewBox='0 0 80 80'><g fill='none' stroke='#FFFFFF' stroke-width='6' stroke-linecap='round' stroke-linejoin='round'>$body</g></svg>"
}
function ConvertTo-Argb([string]$hex) { [uint32]("0xFF" + $hex.TrimStart('#')) }

$app = Join-Path $env:LOCALAPPDATA "Logi\LogiPluginService\Applications\Loupedeck72\@_unrealmxbridge"
$profileDir = Get-ChildItem (Join-Path $app "Profiles") -Directory | Select-Object -First 1
$iconDir = Join-Path $profileDir.FullName "ActionIcons"
New-Item -ItemType Directory -Force $iconDir | Out-Null

# Actions placed on the ring (top level and folders), plus any that already have custom icons.
$profileText = Get-Content (Join-Path $profileDir.FullName "ProfileInfo.json") -Raw
$placed = [regex]::Matches($profileText, 'UnrealMxBridge\.(\w+)"') | ForEach-Object { $_.Groups[1].Value }
$existing = Get-ChildItem $iconDir -Filter '$UnrealMxBridge___*.ict' | ForEach-Object { $_.BaseName -replace '.*UnrealMxBridge\.', '' }
$targets = @($placed) + @($existing) | Where-Object { $classGroup.ContainsKey($_) } | Sort-Object -Unique

$service = Get-Process LogiPluginService -ErrorAction SilentlyContinue | Select-Object -First 1
$serviceExe = if ($service) { $service.Path } else { "C:\Program Files\Logi\LogiPluginService\LogiPluginService.exe" }
if ($service) { Stop-Process -Id $service.Id -Force; Start-Sleep 2 }

try {
    foreach ($class in $targets) {
        $color = ConvertTo-Argb $groupColors[$classGroup[$class]]
        $path = Join-Path $iconDir ('$UnrealMxBridge___Loupedeck.UnrealMxBridge.' + $class + '.ict')
        $ict = if (Test-Path $path) { Get-Content $path -Raw | ConvertFrom-Json } else { $null }

        if ($ict) {
            $image = $ict.items | Where-Object itemType -eq "Image" | Select-Object -First 1
            if (-not $image.imageFileName) {
                # Plugin glyph: re-render as a clean monochrome SVG.
                $image.image = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes((Get-GlyphSvg $actions[$class].Glyph)))
            }
            $image.imageColor = $color
        }
        else {
            $ict = [ordered]@{
                backgroundColor = 4279900698   # #1A1A1A
                items = @(
                    [ordered]@{ image = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes((Get-GlyphSvg $actions[$class].Glyph)))
                                imageFileName = $null; imageColor = $color; imageRotation = "None"; isVisible = $true; itemType = "Image"
                                area = [ordered]@{ x = 17; y = 0; width = 65; height = 65 } },
                    [ordered]@{ text = $actions[$class].Name; originalText = $actions[$class].Name; textColor = 4294967295; fontSize = 5
                                fontName = "Brown Logitech Pan Light"; isVisible = $true; itemType = "Text"
                                area = [ordered]@{ x = 0; y = 79; width = 100; height = 16 } }
                )
            }
        }
        [IO.File]::WriteAllText($path, ($ict | ConvertTo-Json -Depth 6), (New-Object Text.UTF8Encoding $false))
        "{0,-26} {1,-10} {2}" -f $class, $classGroup[$class], $groupColors[$classGroup[$class]]
    }
}
finally {
    Start-Process $serviceExe
}
