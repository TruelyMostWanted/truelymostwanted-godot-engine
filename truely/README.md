# Truely Godot Engine

Eigene Version der Godot Engine. Sie bleibt mit [godotengine/godot](https://github.com/godotengine/godot) synchron.

## Branches

| Branch                | Zweck                                                                    |
|-----------------------|--------------------------------------------------------------------------|
| `master`              | Reine Kopie von `upstream/master`. **Keine eigenen Commits.**            |
| `truely-godot-engine` | Die eigene Engine: `master` + alle eigenen Features. Wird gebaut.        |
| `feature/<name>`      | Ein Feature. Zweigt von `truely-godot-engine` ab und wird dorthin gemergt. |

```
upstream/master ──► master ──(merge)──► truely-godot-engine ◄──(merge/PR)── feature/<name>
```

## Remotes

- `origin`: der eigene Fork. Hierhin wird gepusht.
- `upstream`: godotengine/godot. Nur fetch, Push ist gesperrt (`DISABLED`).

## Täglicher Ablauf

```powershell
# Upstream holen und in die eigene Engine mergen
./truely/sync-upstream.ps1 -MergeIntoEngine -Push

# Neues Feature
git switch -c feature/mein-feature truely-godot-engine
# ... committen ...
git push -u origin feature/mein-feature
gh pr create --base truely-godot-engine      # PR nur innerhalb des Forks

# Bauen
./truely/build.ps1                            # Editor
./truely/build.ps1 -Target template_release   # Export-Template
```

## Wichtig

- **PRs:** Auf GitHub vor dem Absenden prüfen, dass als *base repository* `TruelyMostWanted/yours_truely_godot` eingestellt ist und nicht `godotengine/godot`. `gh` ist per `gh repo set-default` auf den Fork eingestellt.
- **Versionskennung:** `build.ps1` setzt `BUILD_NAME` und optional `GODOT_VERSION_STATUS` über Umgebungsvariablen. `version.py` bleibt unverändert, damit beim Sync keine Konflikte entstehen.
- **Konflikte beim Sync:** `git rerere` ist aktiv. Ein einmal gelöster Konflikt wird beim nächsten Merge automatisch wieder so gelöst.
- **Ein Feature doch an Godot beitragen:** Dafür einen eigenen Branch auf Basis von `upstream/master` anlegen (nicht von `truely-godot-engine`) und den PR bewusst gegen `godotengine/godot` öffnen.
