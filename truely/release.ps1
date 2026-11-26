<#
.SYNOPSIS
    Releases the current state of truely-godot-engine: fast-forwards the release branch to it.

.DESCRIPTION
    truely-godot-engine is the development branch; truely-godot-engine-releases holds only released
    states. This script moves the release branch to a commit of truely-godot-engine (default: its
    tip on origin) with a plain push, which only succeeds as a fast-forward. The push starts the
    full CI on the release branch; when it is green, the workflow "Truely Release" builds all
    editors and export templates and publishes the GitHub release with generated notes.

    Before a release: the CI of that commit on truely-godot-engine should be green,
    truely/godot-status.txt must name the Godot status of the base (beta5, rc1, stable ...), and
    truely/version.txt must hold a Truely version that has no release yet. The release is named
    Truely-v<truely>_Godot<godot>-<status>_<hash> with the tag truely-v<truely>-godot<godot>-<status>;
    the script stops if that tag already exists on another commit (raise truely/version.txt first:
    MINOR for features, PATCH for fixes).

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

# Files of the commit to release, not of the working tree.
function Get-FileAt([string]$path) {
    $text = (& git show "${sha}:$path") -join "`n"
    if ($LASTEXITCODE -ne 0) { throw "$path is missing in $Commit." }
    return $text
}
$status = (Get-FileAt 'truely/godot-status.txt').Trim()
$truely = (Get-FileAt 'truely/version.txt').Trim()
if ($truely -notmatch '^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$') { throw "truely/version.txt holds '$truely', expected MAJOR.MINOR.PATCH." }
$versionPy = Get-FileAt 'version.py'
$v = @{}
foreach ($key in 'major', 'minor', 'patch') {
    if ($versionPy -notmatch "(?m)^$key = (\d+)") { throw "version.py has no $key." }
    $v[$key] = [int]$Matches[1]
}
$godot = "$($v.major).$($v.minor)" + $(if ($v.patch) { ".$($v.patch)" } else { '' })
$tag = "truely-v$truely-godot$godot-$status"
& git rev-parse -q --verify "refs/tags/$tag" *> $null
if ($LASTEXITCODE -eq 0) {
    $tagged = (& git rev-parse "$tag^{commit}").Trim()
    if ($tagged -ne $sha) { throw "Tag $tag already exists on $($tagged.Substring(0, 7)). Raise truely/version.txt (MINOR for features, PATCH for fixes) first." }
}
Write-Host "==> Release Truely v$truely (Godot $godot $status) at $($sha.Substring(0, 7)), tag $tag" -ForegroundColor Cyan

& git push origin "${sha}:refs/heads/$ReleaseBranch"
if ($LASTEXITCODE -ne 0) { throw "Push to $ReleaseBranch failed (not a fast-forward?)." }
Write-Host "Done. CI runs on $ReleaseBranch; when it is green, the release is built and published." -ForegroundColor Green
Write-Host 'Follow it: gh run list -b truely-godot-engine-releases' -ForegroundColor Green
