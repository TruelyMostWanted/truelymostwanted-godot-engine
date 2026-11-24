<#
.SYNOPSIS
    Brings upstream Godot and the carried upstream pull requests into truely-godot-engine, in a fixed order.

.DESCRIPTION
    The chain, top to bottom:
      1. fetch origin and upstream
      2. master <- upstream/master             (fast-forward only; master stays a pure copy)
      3. truely-godot-engine <- master
      4. for each line of truely/upstream-prs.tsv, in file order:
           <branch> <- upstream refs/pull/<pr>/head   (fast-forward, or a merge commit if the PR was rebased)
           truely-godot-engine <- <branch>
      5. with -Push: push master, the PR branches and truely-godot-engine to origin (never forced)

    The script stops at the first conflict and says where it is. Resolve it, commit, and run the
    script again: finished steps are no-ops. git rerere records the resolution for next time.
    Feature branches are not touched; they get truely-godot-engine when they are integrated.

.PARAMETER Check
    Only report what would happen (merge-tree probes); changes nothing except fetching.

.PARAMETER Push
    Push the results to origin.

.PARAMETER Trailer
    Extra line appended to every merge commit message (e.g. a Co-Authored-By line).

.EXAMPLE
    ./truely/merge-chain.ps1 -Check
    ./truely/merge-chain.ps1 -Push
#>
param(
    [switch]$Check,
    [switch]$Push,
    [string]$Trailer = ''
)

$ErrorActionPreference = 'Stop'
$EngineBranch = 'truely-godot-engine'
$Root = (git rev-parse --show-toplevel)
$PrList = Join-Path $Root 'truely/upstream-prs.tsv'

function Invoke-Git {
    & git @args
    if ($LASTEXITCODE -ne 0) { throw "git $($args -join ' ') failed (exit $LASTEXITCODE)." }
}

function Step([string]$text) { Write-Host "==> $text" -ForegroundColor Cyan }

function Get-WorktreeOf([string]$branch) {
    $wt = $null
    foreach ($line in (git worktree list --porcelain)) {
        if ($line -like 'worktree *') { $wt = $line.Substring(9) }
        if ($line -eq "branch refs/heads/$branch") { return $wt }
    }
    return $null
}

function Get-Message([string]$subject) {
    if ($Trailer) { return "$subject`n`n$Trailer" }
    return $subject
}

# True if <from> is already contained in <into>.
function Test-Contains([string]$into, [string]$from) {
    & git merge-base --is-ancestor $from $into
    return ($LASTEXITCODE -eq 0)
}

# Probe a merge without touching any tree. Returns the conflicting paths (empty = clean).
function Get-MergeConflicts([string]$into, [string]$from) {
    $out = & git merge-tree --write-tree --name-only $into $from
    if ($LASTEXITCODE -eq 0) { return @() }
    return @($out | Select-Object -Skip 1 | Where-Object { $_ -and $_ -notlike 'Auto-merging*' -and $_ -notlike 'CONFLICT*' })
}

# Merge <from> into the engine branch, in the worktree that has it checked out.
function Merge-IntoEngine([string]$from, [string]$label) {
    if (Test-Contains $EngineBranch $from) { Write-Host "    ${label}: already in $EngineBranch"; return }
    $conflicts = Get-MergeConflicts $EngineBranch $from
    if ($Check) {
        if ($conflicts.Count) { Write-Host "    ${label}: CONFLICTS in $($conflicts -join ', ')" -ForegroundColor Yellow }
        else { Write-Host "    ${label}: merges cleanly" }
        return
    }
    $dir = Get-WorktreeOf $EngineBranch
    if (-not $dir) { throw "$EngineBranch is not checked out in any worktree. Check it out (e.g. git switch $EngineBranch) and run again." }
    Push-Location $dir
    try {
        if (git status --porcelain --untracked-files=no) { throw "Worktree $dir has uncommitted changes." }
        & git merge --no-ff $from -m (Get-Message "Merge $label into $EngineBranch")
        if ($LASTEXITCODE -ne 0) {
            Write-Host ''
            Write-Host "Conflicts while merging $label in $dir. Resolve them, 'git add' the files, 'git commit --no-edit', then run this script again." -ForegroundColor Yellow
            exit 1
        }
    } finally { Pop-Location }
}

# Bring a carried PR branch up to date with the PR head on upstream.
function Update-PrBranch([string]$branch, [string]$pr) {
    Invoke-Git fetch -q upstream "pull/$pr/head"
    $head = (git rev-parse FETCH_HEAD)
    & git show-ref --verify -q "refs/heads/$branch"
    if ($LASTEXITCODE -ne 0) {
        if ($Check) { Write-Host "    ${branch}: would be created from PR #$pr"; return $head }
        Invoke-Git branch $branch $head
        Write-Host "    ${branch}: created from PR #$pr"
        return $branch
    }
    if (Test-Contains $branch $head) { Write-Host "    ${branch}: up to date with PR #$pr"; return $branch }
    if (Test-Contains $head $branch) {
        if (-not $Check) {
            if (Get-WorktreeOf $branch) { throw "$branch is checked out in a worktree; detach it first." }
            Invoke-Git update-ref "refs/heads/$branch" $head
        }
        Write-Host "    ${branch}: fast-forward to PR #$pr"
        return $(if ($Check) { $head } else { $branch })
    }
    # The PR was rebased or force-pushed: keep our history, record a merge commit.
    $out = & git merge-tree --write-tree $branch $head
    if ($LASTEXITCODE -ne 0) { throw "$branch and the new head of PR #$pr conflict. Merge FETCH_HEAD into $branch by hand, then run again." }
    if ($Check) { Write-Host "    ${branch}: PR #$pr was rebased, a merge commit would update it"; return $head }
    $tree = ($out | Select-Object -First 1)
    $commit = (git commit-tree $tree -p $branch -p $head -m (Get-Message "Merge new head of godotengine/godot#$pr into $branch"))
    Invoke-Git update-ref "refs/heads/$branch" $commit
    Write-Host "    ${branch}: PR #$pr was rebased, merged its new head"
    return $branch
}

Step 'Fetch origin + upstream'
Invoke-Git fetch -q --all --prune

Step 'master <- upstream/master (fast-forward only)'
$ahead = [int](git rev-list --count upstream/master..master)
if ($ahead -gt 0) { throw "master has $ahead own commits. master must stay a pure copy of upstream." }
$behind = [int](git rev-list --count master..upstream/master)
if ($behind -eq 0) { Write-Host '    master: up to date' }
elseif ($Check) { Write-Host "    master: $behind commits behind, would fast-forward" }
else {
    $masterDir = Get-WorktreeOf 'master'
    if ($masterDir) { Push-Location $masterDir; try { Invoke-Git merge -q --ff-only upstream/master } finally { Pop-Location } }
    else { Invoke-Git fetch -q upstream master:master }
    Write-Host "    master: fast-forwarded $behind commits"
}

Step "$EngineBranch <- master"
# In -Check mode master may not be moved yet: probe with upstream/master instead.
Merge-IntoEngine $(if ($Check) { 'upstream/master' } else { 'master' }) 'master'

$prs = @()
if (Test-Path $PrList) {
    $prs = Get-Content $PrList | Where-Object { $_ -and -not $_.StartsWith('#') } | ForEach-Object {
        $f = $_ -split "`t"
        [pscustomobject]@{ Branch = $f[0]; Pr = $f[1] }
    }
}
foreach ($p in $prs) {
    Step "$($p.Branch) (godotengine/godot#$($p.Pr))"
    $ref = Update-PrBranch $p.Branch $p.Pr
    Merge-IntoEngine $ref $p.Branch
}

if ($Push -and -not $Check) {
    Step 'Push to origin'
    Invoke-Git push -q origin master
    foreach ($p in $prs) { Invoke-Git push -q origin $p.Branch }
    Invoke-Git push -q origin $EngineBranch
}

$statusFile = Join-Path $Root 'truely/godot-status.txt'
$status = if (Test-Path $statusFile) { (Get-Content $statusFile -TotalCount 1).Trim() } else { '(missing)' }
Write-Host "Reminder: truely/godot-status.txt says '$status'. If Godot moved to a new status (e.g. beta6, rc1, stable), update it." -ForegroundColor Yellow
Write-Host 'Done.' -ForegroundColor Green
