<#
.SYNOPSIS
    Holt den aktuellen Stand von godotengine/godot und bringt ihn in die eigene Engine.

.DESCRIPTION
    1. fetcht origin und upstream
    2. spult master per Fast-forward auf upstream/master vor und pusht ihn nach origin
    3. merged master in truely-godot-engine (nur mit -MergeIntoEngine)

    master bleibt dabei eine reine Kopie von upstream. Eigene Commits gehoeren
    ausschliesslich auf truely-godot-engine bzw. auf Feature-Branches.

.PARAMETER MergeIntoEngine
    master zusaetzlich in truely-godot-engine mergen. Laeuft in dem Worktree, in dem
    truely-godot-engine ausgecheckt ist (oder wechselt temporaer dorthin).

.PARAMETER Push
    Ergebnis nach origin pushen.

.EXAMPLE
    ./truely/sync-upstream.ps1 -MergeIntoEngine -Push
#>
param(
    [switch]$MergeIntoEngine,
    [switch]$Push
)

$ErrorActionPreference = 'Stop'
$EngineBranch = 'truely-godot-engine'

function Invoke-Git {
    & git @args
    if ($LASTEXITCODE -ne 0) { throw "git $($args -join ' ') ist fehlgeschlagen (Exit $LASTEXITCODE)." }
}

Write-Host '==> Fetch origin + upstream' -ForegroundColor Cyan
Invoke-Git fetch --all --prune

# --- master auf upstream/master vorspulen ---------------------------------
$ahead = [int](git rev-list --count upstream/master..master)
if ($ahead -gt 0) {
    throw "master hat $ahead eigene Commits, die nicht in upstream sind. master muss eine reine Kopie von upstream bleiben - verschiebe die Commits auf einen Feature-Branch."
}

function Get-WorktreeOf([string]$branch) {
    $wt = $null
    foreach ($line in (git worktree list --porcelain)) {
        if ($line -like 'worktree *') { $wt = $line.Substring(9) }
        if ($line -eq "branch refs/heads/$branch") { return $wt }
    }
    return $null
}

$current = git branch --show-current
Write-Host '==> master <- upstream/master (fast-forward)' -ForegroundColor Cyan
$masterDir = Get-WorktreeOf 'master'
if ($masterDir) {
    # master ist in einem Worktree ausgecheckt -> dort vorspulen.
    Push-Location $masterDir
    try { Invoke-Git merge --ff-only upstream/master } finally { Pop-Location }
} else {
    # Aktualisiert den lokalen master, ohne ihn auszuchecken (nur Fast-forward erlaubt).
    Invoke-Git fetch upstream master:master
}
if ($Push) { Invoke-Git push origin master }

if (-not $MergeIntoEngine) {
    Write-Host 'Fertig. (truely-godot-engine nicht angefasst - nutze -MergeIntoEngine)' -ForegroundColor Green
    return
}

# --- master in truely-godot-engine mergen ----------------------------------
$engineDir = Get-WorktreeOf $EngineBranch

$switchedBack = $null
if (-not $engineDir) {
    if (git status --porcelain --untracked-files=no) { throw 'Arbeitsverzeichnis hat Aenderungen - bitte erst committen oder stashen.' }
    $switchedBack = $current
    Invoke-Git switch $EngineBranch
    $engineDir = (Get-Location).Path
}

Push-Location $engineDir
try {
    if (git status --porcelain --untracked-files=no) { throw "Worktree $engineDir hat uncommittete Aenderungen." }
    Write-Host "==> $EngineBranch <- master  (in $engineDir)" -ForegroundColor Cyan
    & git merge master --no-edit
    if ($LASTEXITCODE -ne 0) {
        Write-Host ''
        Write-Host "Merge-Konflikte in $engineDir. Loesen, dann 'git add <dateien>' und 'git commit --no-edit'." -ForegroundColor Yellow
        Write-Host '(rerere ist aktiv: einmal geloeste Konflikte werden beim naechsten Mal automatisch wiederverwendet.)' -ForegroundColor Yellow
        $switchedBack = $null   # nicht wegwechseln, solange der Merge offen ist
        exit 1
    }
    if ($Push) { Invoke-Git push origin $EngineBranch }
} finally {
    Pop-Location
    if ($switchedBack) { Invoke-Git switch $switchedBack }
}

Write-Host 'Fertig.' -ForegroundColor Green
