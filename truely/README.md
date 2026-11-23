# Truely Godot Engine

Eigene Version der Godot Engine. Sie bleibt mit [godotengine/godot](https://github.com/godotengine/godot) synchron.

## Branches

```
upstream/master
 │
 └─► master                      reine Kopie von upstream, keine eigenen Commits
      ├─► <pr-branch>            Beitrag an Godot  ──► PR an godotengine/godot
      ├─► <pr-branch>                 │
      │                               └──(merge)──┐
      └──(merge)──► truely-godot-engine ◄─────────┘   die eigene Engine, wird gebaut
                      ├─► <feature-branch>        nur für die eigene Engine
                      └─► <feature-branch>
```

| Branch                | Zweigt ab von         | Wird gemergt in                                   |
|-----------------------|-----------------------|---------------------------------------------------|
| `master`              | `upstream/master`     | nur Fast-forward von upstream                     |
| `truely-godot-engine` | `master`              | – (erhält `master`, PR- und Feature-Branches)     |
| PR-Branch             | `master`              | PR an Godot **und** `truely-godot-engine`         |
| Feature-Branch        | `truely-godot-engine` | `truely-godot-engine`                             |

Feature-Branches tragen als Präfix die Nummer ihres Issues, mindestens zweistellig, z. B. `01-feature-resource-paths-retrieval` (siehe `CONVENTIONS.md`). Die Namen der PR-Branches sind frei wählbar. Präfixe wie `pr/…` oder `feature/…` sind optional. Viele Git-Tools zeigen sie als Ordner an.

**Warum PR-Branches von `master` abzweigen müssen:** Ein PR enthält alle Commits seines Branches, die noch nicht in `upstream/master` sind. Ein Branch von `truely-godot-engine` würde alle eigenen Features mit in den PR ziehen.

## Remotes

- `origin`: der eigene Fork. Hierhin wird gepusht.
- `upstream`: godotengine/godot. Nur fetch, Push ist gesperrt (`DISABLED`).

## Täglicher Ablauf

```powershell
# Upstream holen und in die eigene Engine mergen
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

## Wichtig

- **PRs auf GitHub:** Vor dem Absenden das *base repository* prüfen: `TruelyMostWanted/truelymostwanted-godot-engine` für interne PRs, `godotengine/godot` nur für Beiträge. `gh` ist per `gh repo set-default` auf den Fork eingestellt.
- **PR-Branches aktuell halten:** Bei Konflikten mit Godot den PR-Branch per Rebase auf `upstream/master` bringen (`git rebase upstream/master`), dann `git push --force-with-lease`.
- **Gemeinsamer SCons-Cache:** Ist `TRUELY_SCONS_CACHE` gesetzt (einmal pro Rechner: `setx TRUELY_SCONS_CACHE P:\scons-cache`), hängt `build.ps1` `cache_path=<Ordner>` und `cache_limit=40` (GiB, änderbar über `TRUELY_SCONS_CACHE_LIMIT`) an. Alle Arbeitsbäume, auch die Git-Worktrees der Agenten, nutzen dann dieselben Objektdateien und bauen nur, was sich geändert hat.
- **Versionskennung:** `build.ps1` setzt `BUILD_NAME` und optional `GODOT_VERSION_STATUS` über Umgebungsvariablen. `version.py` bleibt unverändert, damit beim Sync keine Konflikte entstehen.
- **Konflikte beim Sync:** `git rerere` ist aktiv. Ein einmal gelöster Konflikt wird beim nächsten Merge automatisch wieder so gelöst.
