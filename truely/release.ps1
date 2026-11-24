<#
.SYNOPSIS
    Releases the current state of truely-godot-engine: fast-forwards the release branch to it.

.DESCRIPTION
    truely-godot-engine is the development branch; truely-godot-engine-releases holds only released
    states. This script moves the release branch to a commit of truely-godot-engine (default: its
    tip on origin) with a plain push, which only succeeds as a fast-forward. The push starts the
    full CI on the release branch; when it is green, the workflow "Truely Release" builds all
    editors and export templates and publishes the GitHub release with generated notes.

    Before a release: the CI of that commit on truely-godot-engine should be green, and
    truely/godot-status.txt must name the Godot status of the base (beta5, rc1, stable ...).

.PARAMETER Commit
    Commit of truely-godot-engine to release. Default: origin/truely-godot-engine.

.EXAMPLE
    ./truely/release.ps1
    ./truely/release.ps1 -Commit 3acb141
#>
param(
    [string]$Commit = 'origin/truely-godot-engine'
)

$ErrorActionPreference = 'Stop'
$ReleaseBranch = 'truely-godot-engine-releases'

& git fetch -q origin
if ($LASTEXITCODE -ne 0) { throw 'git fetch failed.' }
$sha = (& git rev-parse --verify "$Commit^{commit}").Trim()
& git merge-base --is-ancestor $sha origin/truely-godot-engine
if ($LASTEXITCODE -ne 0) { throw "$Commit is not part of truely-godot-engine." }

$status = (Get-Content (Join-Path (git rev-parse --show-toplevel) 'truely/godot-status.txt') -Raw).Trim()
Write-Host "==> Release $($sha.Substring(0, 7)) (Godot status '$status' from truely/godot-status.txt)" -ForegroundColor Cyan

& git push origin "${sha}:refs/heads/$ReleaseBranch"
if ($LASTEXITCODE -ne 0) { throw "Push to $ReleaseBranch failed (not a fast-forward?)." }
Write-Host "Done. CI runs on $ReleaseBranch; when it is green, the release is built and published." -ForegroundColor Green
Write-Host 'Follow it: gh run list -b truely-godot-engine-releases' -ForegroundColor Green
