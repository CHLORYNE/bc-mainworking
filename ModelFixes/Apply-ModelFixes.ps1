# Copies the corrected boat.ini files into the simulator's Models folder.
# Each boat.ini that gets replaced is first saved next to it as boat.ini.bak
# (only once, so running the script again keeps the original backup).
#
#   Apply-ModelFixes.ps1                     -> bin\Models of this repository
#   Apply-ModelFixes.ps1 -ModelsPath D:\X    -> another Models folder
#   Apply-ModelFixes.ps1 -Restore            -> put the .bak files back
param(
    [string]$ModelsPath = (Join-Path $PSScriptRoot "..\bin\Models"),
    [switch]$Restore
)

$src = Join-Path $PSScriptRoot "Models"
if (-not (Test-Path -LiteralPath $ModelsPath)) {
    Write-Host "Models folder not found: $ModelsPath" -ForegroundColor Red
    Write-Host "Run again with -ModelsPath <path to your Models folder>."
    exit 1
}
$ModelsPath = (Resolve-Path -LiteralPath $ModelsPath).Path
Write-Host "Models folder: $ModelsPath"

$done = 0; $missing = 0
foreach ($file in Get-ChildItem -LiteralPath $src -Recurse -Filter "boat.ini") {
    $rel = $file.FullName.Substring($src.Length).TrimStart('\', '/')
    $dst = Join-Path $ModelsPath $rel
    $bak = "$dst.bak"

    if ($Restore) {
        if (Test-Path -LiteralPath $bak) {
            Copy-Item -LiteralPath $bak -Destination $dst -Force
            Write-Host "restored  $rel"
            $done++
        }
        continue
    }

    if (-not (Test-Path -LiteralPath (Split-Path $dst -Parent))) {
        Write-Host "not found $rel (ship folder missing, skipped)" -ForegroundColor Yellow
        $missing++
        continue
    }
    if ((Test-Path -LiteralPath $dst) -and -not (Test-Path -LiteralPath $bak)) {
        Copy-Item -LiteralPath $dst -Destination $bak
    }
    Copy-Item -LiteralPath $file.FullName -Destination $dst -Force
    Write-Host "updated   $rel"
    $done++
}

if ($Restore) { Write-Host "$done file(s) restored." -ForegroundColor Green }
else { Write-Host "$done file(s) updated, $missing skipped." -ForegroundColor Green }
