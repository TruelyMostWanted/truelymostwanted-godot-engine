<#
.SYNOPSIS
    Baut die eigene Engine-Version (Editor oder Export-Templates) mit SCons.

.DESCRIPTION
    Kennzeichnet den Build als eigene Version, ohne version.py zu aendern
    (so entstehen beim Upstream-Sync keine Konflikte):
      BUILD_NAME           -> z. B. "v4.7.dev.truely.<hash>"
      GODOT_VERSION_STATUS -> optional, z. B. "truely" statt "dev"/"stable"

    Zusaetzliche SCons-Optionen werden einfach angehaengt, z. B. module_mono_enabled=yes.
    Lokale Dauer-Optionen kannst du in custom.py im Repo-Root ablegen (ist in .gitignore).

.EXAMPLE
    ./truely/build.ps1                                  # Editor, Windows
    ./truely/build.ps1 -Target template_release         # Export-Template
    ./truely/build.ps1 -Status truely module_mono_enabled=yes
#>
[CmdletBinding(PositionalBinding = $false)]
param(
    [ValidateSet('editor', 'template_debug', 'template_release')]
    [string]$Target = 'editor',
    [string]$Platform = 'windows',
    [string]$BuildName = 'truely',
    [string]$Status = '',
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$SconsArgs
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    $env:BUILD_NAME = $BuildName
    if ($Status) { $env:GODOT_VERSION_STATUS = $Status } else { Remove-Item Env:GODOT_VERSION_STATUS -ErrorAction SilentlyContinue }

    # Shared SCons cache for all working trees (git worktrees of agents reuse the objects).
    # Set TRUELY_SCONS_CACHE once per machine, e.g. setx TRUELY_SCONS_CACHE P:\scons-cache
    $hasCache = @($SconsArgs | Where-Object { $_ -like 'cache_path=*' }).Count -gt 0
    if ($env:TRUELY_SCONS_CACHE -and -not $hasCache) {
        $limit = if ($env:TRUELY_SCONS_CACHE_LIMIT) { $env:TRUELY_SCONS_CACHE_LIMIT } else { '40' }
        $SconsArgs = @($SconsArgs) + @("cache_path=$($env:TRUELY_SCONS_CACHE)", "cache_limit=$limit")
    }
    $jobs = [Environment]::ProcessorCount
    Write-Host "==> scons platform=$Platform target=$Target (BUILD_NAME=$BuildName) $SconsArgs" -ForegroundColor Cyan
    & scons "platform=$Platform" "target=$Target" "-j$jobs" @SconsArgs
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
