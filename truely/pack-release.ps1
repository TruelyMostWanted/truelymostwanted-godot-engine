<#
.SYNOPSIS
    Packt die lokalen Windows-Builds aus bin/ als Release-Dateien (Editor-Zips und .tpz).

.DESCRIPTION
    Erwartet in bin/ die Builds von ./truely/build.ps1 (production=yes empfohlen):
      godot.windows.editor.x86_64[.console].exe                 Editor
      godot.windows.editor.x86_64.mono[.console].exe + GodotSharp/   Editor (Mono)
      godot.windows.template_{debug,release}.x86_64[.mono][.console].exe   Templates

    Der Name kommt aus "<exe> --truely-version" (z. B. 0.1.0) und "<exe> --version"
    (z. B. 4.8.beta5.truely.a1b2c3d4e):
      Truely-v0.1.0_Godot4.8-beta5_a1b2c3d_win64.zip
      Truely-v0.1.0_Godot4.8-beta5_a1b2c3d_mono_win64.zip
      Truely-v0.1.0_Godot4.8-beta5_a1b2c3d_export_templates.tpz        (nur Windows-Templates)
      Truely-v0.1.0_Godot4.8-beta5_a1b2c3d_mono_export_templates.tpz   (nur Windows-Templates)

    Jede Editor-Zip enthaelt einen Ordner mit den Exes und ._sc_ (Self-contained-Modus:
    Einstellungen und Export-Templates bleiben neben der Exe). Die .tpz enthalten templates/
    mit version.txt (z. B. 4.8.beta5 bzw. 4.8.beta5.mono). Fehlende Builds werden uebersprungen.

    Die volle Release mit Linux-, macOS- und Web-Templates baut .github/workflows/truely_release.yml.

.PARAMETER BinDir
    Ordner mit den Builds. Standard: bin/ im Repo.

.PARAMETER OutDir
    Zielordner. Standard: P:/Projects/GitHub/truely-releases/<Truely-v...>/

.PARAMETER Version
    Nur zum Testen: Versionsstring statt "<exe> --version" (z. B. 4.8.beta5.truely.a1b2c3d4e).

.PARAMETER TruelyVersion
    Nur zum Testen: Truely-Version statt "<exe> --truely-version" (z. B. 0.1.0).

.EXAMPLE
    ./truely/pack-release.ps1
    ./truely/pack-release.ps1 -OutDir D:/releases
#>
[CmdletBinding()]
param(
    [string]$BinDir = '',
    [string]$OutDir = '',
    [string]$Version = '',
    [string]$TruelyVersion = ''
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$root = Split-Path -Parent $PSScriptRoot
if (-not $BinDir) { $BinDir = Join-Path $root 'bin' }
$BinDir = (Resolve-Path $BinDir).Path

function Get-Bin([string]$name) {
    $p = Join-Path $BinDir $name
    if (Test-Path $p) { return $p }
    return $null
}

# "4.8.beta5.mono.truely.a1b2c3d4e" -> Config 4.8.beta5.mono, Base 4.8-beta5, Hash a1b2c3d
function Read-Version([string]$exe, [string]$given, [string]$givenTruely) {
    # The console wrapper forwards stdout; the GUI exe does not print to a pipe.
    $console = $exe -replace '\.exe$', '.console.exe'
    $run = if (Test-Path $console) { $console } else { $exe }
    $text = $given
    if (-not $text) {
        $text = (& $run --version 2>$null | Where-Object { $_ -match '^\d+\.\d+' } | Select-Object -Last 1)
        if (-not $text) { throw "$run --version printed no version." }
    }
    $parts = $text.Trim() -split '\.'
    $i = [Array]::IndexOf($parts, 'truely')
    if ($i -lt 3) { throw "Version '$text' has no build name 'truely'. Build with ./truely/build.ps1." }
    $config = $parts[0..($i - 1)]
    $numbers = @($config | Where-Object { $_ -match '^\d+$' })
    $status = $config[$numbers.Count]
    $hash = if ($parts.Count -gt $i + 1) { $parts[$i + 1] } else { '' }
    if ($hash.Length -gt 7) { $hash = $hash.Substring(0, 7) }
    if (-not $hash) { throw "Version '$text' has no commit hash." }
    $truely = $givenTruely
    if (-not $truely) {
        # Builds before #265 do not know the option and would start the editor instead: ask --help first.
        if (-not (& $run --help 2>$null | Select-String -SimpleMatch -Quiet -Pattern '--truely-version')) {
            throw "$run has no --truely-version (built before #265). Rebuild with ./truely/build.ps1 or pass -TruelyVersion."
        }
        $truely =(& $run --truely-version 2>$null | Where-Object { $_ -match '^\d+\.\d+\.\d+$' } | Select-Object -Last 1)
        if (-not $truely) { throw "$run --truely-version printed no version. Build with ./truely/build.ps1 (#265 or newer)." }
    }
    return [pscustomobject]@{
        Text   = $text.Trim()
        Config = ($config -join '.')
        Base   = "$($numbers -join '.')-$status"
        Hash   = $hash
        Truely = $truely.Trim()
    }
}

# Zip with forward slashes in entry names (Compress-Archive in PowerShell 5.1 writes backslashes).
function New-Zip([string]$sourceDir, [string]$zipPath) {
    if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
    $zip = [System.IO.Compression.ZipFile]::Open($zipPath, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        $baseLen = (Resolve-Path $sourceDir).Path.TrimEnd('\', '/').Length + 1
        foreach ($f in Get-ChildItem $sourceDir -Recurse -File -Force) {
            $entry = $f.FullName.Substring($baseLen).Replace('\', '/')
            [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $f.FullName, $entry, [System.IO.Compression.CompressionLevel]::Optimal)
        }
    } finally {
        $zip.Dispose()
    }
    Write-Host "    $(Split-Path -Leaf $zipPath)"
}

$stdEditor = Get-Bin 'godot.windows.editor.x86_64.exe'
$monoEditor = Get-Bin 'godot.windows.editor.x86_64.mono.exe'
$stdTemplate = Get-Bin 'godot.windows.template_release.x86_64.exe'
$monoTemplate = Get-Bin 'godot.windows.template_release.x86_64.mono.exe'

$std = $null
$mono = $null
if ($stdEditor) { $std = Read-Version $stdEditor $Version $TruelyVersion }
if ($monoEditor) { $mono = Read-Version $monoEditor $(if ($Version) { $Version -replace '^(\d+\.\d+(?:\.\d+)?\.[^.]+)', '$1.mono' } else { '' }) $TruelyVersion }
$ref = if ($std) { $std } elseif ($mono) { $mono } else { throw "No editor build in $BinDir." }
if ($std -and $mono -and ($std.Hash -ne $mono.Hash -or $std.Base -ne $mono.Base -or $std.Truely -ne $mono.Truely)) {
    throw "Standard ($($std.Text)) and Mono ($($mono.Text)) editors come from different builds. Rebuild one of them."
}

$name = "Truely-v$($ref.Truely)_Godot$($ref.Base)_$($ref.Hash)"
if (-not $OutDir) { $OutDir = "P:/Projects/GitHub/truely-releases/$name" }
New-Item -ItemType Directory -Force $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
$stage = Join-Path ([System.IO.Path]::GetTempPath()) "truely-pack-$PID"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null

Write-Host "==> $name -> $OutDir" -ForegroundColor Cyan
try {
    # Editors: one folder per zip with the exes and ._sc_ (self-contained mode).
    foreach ($e in @(
            @{ Exe = $stdEditor; Suffix = 'win64'; Mono = $false },
            @{ Exe = $monoEditor; Suffix = 'mono_win64'; Mono = $true })) {
        if (-not $e.Exe) { Write-Host "    skip $($e.Suffix) editor (not built)"; continue }
        $folder = "${name}_$($e.Suffix)"
        $dir = Join-Path (Join-Path $stage $e.Suffix) $folder
        New-Item -ItemType Directory -Force $dir | Out-Null
        Copy-Item $e.Exe (Join-Path $dir "$folder.exe")
        $console = $e.Exe -replace '\.exe$', '.console.exe'
        if (Test-Path $console) { Copy-Item $console (Join-Path $dir "${folder}_console.exe") }
        New-Item -ItemType File (Join-Path $dir '._sc_') | Out-Null
        if ($e.Mono) {
            $sharp = Join-Path $BinDir 'GodotSharp'
            if (-not (Test-Path $sharp)) { throw "Mono editor without $sharp. Run build_assemblies.py first." }
            Copy-Item $sharp (Join-Path $dir 'GodotSharp') -Recurse
        }
        New-Zip (Join-Path $stage $e.Suffix) (Join-Path $OutDir "$folder.zip")
    }

    # Export templates (Windows only): templates/ with version.txt, named like the official ones.
    foreach ($t in @(
            @{ Probe = $stdTemplate; Mono = ''; Prefix = ''; Info = $std },
            @{ Probe = $monoTemplate; Mono = '.mono'; Prefix = 'mono_'; Info = $mono })) {
        if (-not $t.Probe) { Write-Host "    skip $($t.Prefix)export templates (not built)"; continue }
        $config = if ($t.Info) { $t.Info.Config } elseif ($t.Mono) { "$($std.Config).mono" } else { $mono.Config -replace '\.mono$', '' }
        $dir = Join-Path (Join-Path $stage "$($t.Prefix)tpz") 'templates'
        New-Item -ItemType Directory -Force $dir | Out-Null
        foreach ($target in 'debug', 'release') {
            foreach ($console in '', '.console') {
                $src = Get-Bin "godot.windows.template_$target.x86_64$($t.Mono)$console.exe"
                if (-not $src) { throw "Missing template godot.windows.template_$target.x86_64$($t.Mono)$console.exe." }
                Copy-Item $src (Join-Path $dir "windows_${target}_x86_64$($console -replace '\.', '_').exe")
            }
        }
        [System.IO.File]::WriteAllText((Join-Path $dir 'version.txt'), $config)
        New-Zip (Join-Path $stage "$($t.Prefix)tpz") (Join-Path $OutDir "${name}_$($t.Prefix)export_templates.tpz")
    }
} finally {
    Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
}
Write-Host 'Done. Local packs contain Windows export templates only.' -ForegroundColor Green
