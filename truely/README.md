# Truely Godot Engine

Eigene Version der Godot Engine. Sie bleibt mit [godotengine/godot](https://github.com/godotengine/godot) synchron.

## Branches

```
upstream/master
 │
 └─► master                               reine Kopie von upstream, keine eigenen Commits
      ├─► 195-feature-opus-audio, …        mitgeführte Godot-PRs (upstream-prs.tsv), merge-chain.ps1
      ├─► <pr-branch>                      Beitrag an Godot  ──► PR an godotengine/godot
      └──(merge)──► truely-godot-engine    die eigene Engine: Entwicklung, keine CI, keine Releases
                      ├─► <nn>-feature-…   Feature-Branches (Squash-Merge, bleiben bestehen)
                      └──(Fast-forward, release.ps1)──► truely-godot-engine-releases
                                                         freigegebene Stände: CI → Builds → Release
```

| Branch                | Zweigt ab von         | Wird gemergt in                                   |
|-----------------------|-----------------------|---------------------------------------------------|
| `master`              | `upstream/master`     | nur Fast-forward von upstream                     |
| `truely-godot-engine` | `master`              | – (erhält `master`, PR- und Feature-Branches)     |
| PR-Branch             | `master`              | PR an Godot **und** `truely-godot-engine`         |
| Feature-Branch        | `truely-godot-engine` | `truely-godot-engine`                             |
| `truely-godot-engine-releases` | `truely-godot-engine` | – (nur Fast-forward per `release.ps1`; ein grüner CI-Lauf veröffentlicht) |

Feature-Branches tragen als Präfix die Nummer ihres Issues, mindestens zweistellig, z. B. `01-feature-resource-paths-retrieval` (siehe `CONVENTIONS.md`). Die Namen der PR-Branches sind frei wählbar. Präfixe wie `pr/…` oder `feature/…` sind optional. Viele Git-Tools zeigen sie als Ordner an.

**Warum PR-Branches von `master` abzweigen müssen:** Ein PR enthält alle Commits seines Branches, die noch nicht in `upstream/master` sind. Ein Branch von `truely-godot-engine` würde alle eigenen Features mit in den PR ziehen.

## Remotes

- `origin`: der eigene Fork. Hierhin wird gepusht.
- `upstream`: godotengine/godot. Nur fetch, Push ist gesperrt (`DISABLED`).

## Täglicher Ablauf

```powershell
# Upstream und die mitgeführten Godot-PRs in fester Reihenfolge mergen
./truely/merge-chain.ps1 -Check     # nur prüfen
./truely/merge-chain.ps1 -Push

# nur master und Engine (ohne mitgeführte PRs)
./truely/sync-upstream.ps1 -MergeIntoEngine -Push

# Feature nur für die eigene Engine
git switch -c mein-feature truely-godot-engine
git push -u origin mein-feature
gh pr create --base truely-godot-engine                         # PR innerhalb des Forks

# Beitrag an Godot
git switch -c mein-beitrag master
git push -u origin mein-beitrag
gh pr create --repo godotengine/godot --base master             # PR an Godot
git switch truely-godot-engine; git merge mein-beitrag          # sofort in der eigenen Engine nutzen

# Bauen
./truely/build.ps1                            # Editor
./truely/build.ps1 -Target template_release   # Export-Template
```

Wird ein PR in Godot gemergt, kommt er über `master` ohnehin in die eigene Engine. Den PR-Branch kann man danach löschen.

## Mitgeführte Godot-PRs

Offene PRs von Godot, die die eigene Engine schon vor dem Merge in Godot enthält, stehen in [`upstream-prs.tsv`](upstream-prs.tsv) (Branch, PR-Nummer, Issue). `merge-chain.ps1` macht in dieser Reihenfolge:

1. `fetch` von `origin` und `upstream`
2. `master` ← `upstream/master` (nur Fast-forward)
3. `truely-godot-engine` ← `master`
4. je Zeile der Liste: Branch ← `refs/pull/<pr>/head` (Fast-forward; bei einem Rebase des PRs ein Merge-Commit, nie `--force`), dann `truely-godot-engine` ← Branch
5. mit `-Push`: alles nach `origin`

Bei einem Konflikt hält das Skript an; lösen, committen, erneut starten (erledigte Schritte laufen leer). Ein PR, der auf einem anderen aufbaut, steht in der Liste danach. Ist ein PR in Godot gemergt, die Zeile entfernen; `master` bringt ihn dann mit.

## Releases

Entwicklung und Veröffentlichung sind getrennt: `truely-godot-engine` ist der Entwicklungsstand (Merges, lokal geprüft, **keine** CI und **keine** Releases), `truely-godot-engine-releases` enthält nur freigegebene Stände.

```powershell
./truely/release.ps1                  # Spitze von origin/truely-godot-engine freigeben
./truely/release.ps1 -Commit 3acb141  # einen bestimmten, grün getesteten Commit freigeben
```

Das Skript schiebt den Release-Branch per Fast-forward auf den Commit. Der Push startet die volle CI auf `truely-godot-engine-releases`; ist sie grün, startet der Workflow [`truely_release.yml`](../.github/workflows/truely_release.yml) (`workflow_run`) und baut genau diesen Commit (`head_sha`). Er released nur, wenn sich seit dem letzten Release-Tag (`truely-v*`, ältere `truely-godot*`) Dateien außer `*.md` und `.agents/` geändert haben; `doc/classes/` zählt mit. Von Hand geht es weiter mit *Actions → 📦 Truely Release → Run workflow* (Option `draft` für einen Entwurf); das released immer.

- **Release-Notes:** [`release-notes.sh`](../.github/truely/release-notes.sh) schreibt sie aus der Git-Historie: First-Parent-Commits seit dem letzten Release-Tag (ohne Tag: die letzten 50), ohne Merge- und `[skip ci]`-Commits. Die Issue-Nummer `(#N)` am Ende der Betreffzeile wird per `gh` nachgeschlagen und nach Label gruppiert (*Features* = `type: feature`, *Fixes* = `type: bug`, sonst *Refactoring and tasks*). Oben stehen die Truely-Version (z. B. „Truely v0.1.0 → v0.2.0“, aus `truely/version.txt`); dazu ein Link aufs Wiki, die Godot-Basis (z. B. „Godot 4.8 beta4 → beta5“, aus `version.py` und `godot-status.txt`), die mitgeführten Godot-PRs aus [`upstream-prs.tsv`](upstream-prs.tsv), Installationshinweise und der Link auf den vollen Diff. Lokal testen (Git Bash, `gh` angemeldet): `bash .github/truely/release-notes.sh HEAD <tag> 4.8.beta5 [<vorheriger-tag>]`.
- **Pre-Release und Latest:** Ist der Status nicht `stable`, wird das Release als Pre-Release angelegt und nicht „Latest“; nur `stable` wird „Latest“.

- **Inhalt:** Editoren (Standard und Mono) für Windows, Linux und macOS (universal, per `lipo`) sowie Export-Templates (`.tpz` mit `templates/version.txt`) für Windows, Linux, macOS und Web mit und ohne Threads. Gebaut mit `production=yes`, ohne SCons-Cache.
- **Namen (ab #265):** `Truely-v<Truely>_Godot<Godot>-<Status>_<Hash>_<Plattform>`, z. B. `Truely-v0.1.0_Godot4.8-beta5_a1b2c3d_win64.zip`, `…_mono_win64.zip`, `…_export_templates.tpz`, `…_mono_export_templates.tpz`. Reihenfolge: eigene Version, Godot-Basis (Godots Kurzform `4.8`, `4.8.1`), Commit, Plattform. Tag `truely-v0.1.0-godot4.8-beta5` (ohne Hash, zeigt auf den Commit), Titel „Truely v0.1.0 (Godot 4.8 beta 5, a1b2c3d)“. Ältere Releases behalten ihre Namen (`Truely_Godot…`, Tag `truely-godot…`).
- **Truely-Version:** [`version.txt`](version.txt), SemVer `MAJOR.MINOR.PATCH`, Start `0.1.0`. **MINOR** für neue Features, **PATCH** nur für Fehlerbehebungen, **MAJOR** wenn die Godot-Basis auf ein neues Major/Minor wechselt (4.8 → 4.9) oder bei inkompatiblen API-Änderungen. Erhöht wird **einmal pro Release**, beim Merge der ersten Änderung nach einem Release (Regel in `FEATURE-WORKFLOW.md`, 2.5). Gibt es den Tag schon auf einem anderen Commit, brechen `release.ps1` und der Workflow mit „Raise truely/version.txt“ ab. Der Build liest die Datei in `core/truely_version.gen.h` (`core/truely_version_builders.py`); in der Engine: `Engine.get_version_info()` (`truely_major`, `truely_minor`, `truely_patch`, `truely_hex`, `truely_version`), Editor (Über-Dialog, untere Leiste, Projektmanager, Fenstertitel), Startzeile und `--truely-version`. `--version` bleibt Godots einzeilige Ausgabe, weil Werkzeuge sie auswerten.
- **Status der Godot-Basis:** [`godot-status.txt`](godot-status.txt) (z. B. `beta5`). `build.ps1` nimmt ihn, wenn `-Status` fehlt, der Workflow immer. Nach einem Sync, der Godot auf einen neuen Stand bringt (`beta6`, `rc1`, `stable`), die Datei anpassen; `merge-chain.ps1` erinnert am Ende daran. Versionsstring: `4.8.beta5.truely.<hash>` (Mono: `4.8.beta5.mono.truely.<hash>`).
- **Self-contained:** Jede Editor-Zip enthält einen Ordner mit dem Editor und `._sc_`. Einstellungen und Export-Templates bleiben in diesem Ordner, Truely und das offizielle Godot derselben Version überschreiben sich die Templates nicht.
- **macOS:** unsigniert (nur Ad-hoc-Signatur). Erster Start per Rechtsklick → Öffnen; für den Self-contained-Modus einmal `xattr -dr com.apple.quarantine <Ordner>`.
- **Lokal packen:** `./truely/pack-release.ps1` packt die Windows-Builds aus `bin/` mit denselben Namen nach `P:/Projects/GitHub/truely-releases/<Name>/` (Parameter `-OutDir`). Der Name kommt aus `<exe> --truely-version` und `<exe> --version` (Builds vor #265 brechen ab; zum Testen `-TruelyVersion 0.1.0 -Version 4.8.beta5.truely.<hash>`). **Lokale Packs enthalten nur Windows-Templates.** Vorher mit `production=yes` bauen, bei Mono danach Glue und `build_assemblies.py` (`FEATURE-WORKFLOW.md`, Abschnitt 3). Nach einem Wechsel des Status ohne SCons-Cache bauen (`$env:TRUELY_SCONS_CACHE = ''`), sonst landen alte Versionsstrings im Build.
- **Packen im Workflow:** [`package-release.sh`](../.github/truely/package-release.sh) setzt die Artefakte der Build-Jobs zu Zips und `.tpz` zusammen.

## Wichtig

- **PRs auf GitHub:** Vor dem Absenden das *base repository* prüfen: `TruelyMostWanted/truelymostwanted-godot-engine` für interne PRs, `godotengine/godot` nur für Beiträge. `gh` ist per `gh repo set-default` auf den Fork eingestellt.
- **PR-Branches aktuell halten:** Bei Konflikten mit Godot den PR-Branch per Rebase auf `upstream/master` bringen (`git rebase upstream/master`), dann `git push --force-with-lease`.
- **Gemeinsamer SCons-Cache:** Ist `TRUELY_SCONS_CACHE` gesetzt (einmal pro Rechner: `setx TRUELY_SCONS_CACHE P:\scons-cache`), hängt `build.ps1` `cache_path=<Ordner>` und `cache_limit=40` (GiB, änderbar über `TRUELY_SCONS_CACHE_LIMIT`) an. Alle Arbeitsbäume, auch die Git-Worktrees der Agenten, nutzen dann dieselben Objektdateien und bauen nur, was sich geändert hat.
- **Versionskennung:** `build.ps1` setzt `BUILD_NAME` und `GODOT_VERSION_STATUS` (aus `-Status` oder `godot-status.txt`) über Umgebungsvariablen; die Truely-Version kommt aus `version.txt`. `version.py` bleibt unverändert, damit beim Sync keine Konflikte entstehen.
- **Konflikte beim Sync:** `git rerere` ist aktiv. Ein einmal gelöster Konflikt wird beim nächsten Merge automatisch wieder so gelöst.
