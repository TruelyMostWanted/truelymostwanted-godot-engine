# Truely Godot Engine

Eigene Version der Godot Engine. Sie bleibt mit [godotengine/godot](https://github.com/godotengine/godot) synchron.

## Branches

```
upstream/master
 â”‚
 â””â”€â–º master                      reine Kopie von upstream, keine eigenen Commits
      â”œâ”€â–º <pr-branch>            Beitrag an Godot  â”€â”€â–º PR an godotengine/godot
      â”œâ”€â–º <pr-branch>                 â”‚
      â”‚                               â””â”€â”€(merge)â”€â”€â”
      â””â”€â”€(merge)â”€â”€â–º truely-godot-engine â—„â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜   die eigene Engine, wird gebaut
                      â”œâ”€â–º <feature-branch>        nur fÃ¼r die eigene Engine
                      â””â”€â–º <feature-branch>
```

| Branch                | Zweigt ab von         | Wird gemergt in                                   |
|-----------------------|-----------------------|---------------------------------------------------|
| `master`              | `upstream/master`     | nur Fast-forward von upstream                     |
| `truely-godot-engine` | `master`              | â€“ (erhÃ¤lt `master`, PR- und Feature-Branches)     |
| PR-Branch             | `master`              | PR an Godot **und** `truely-godot-engine`         |
| Feature-Branch        | `truely-godot-engine` | `truely-godot-engine`                             |

Die Namen der PR- und Feature-Branches sind frei wÃ¤hlbar. PrÃ¤fixe wie `pr/â€¦` oder `feature/â€¦` sind optional. Viele Git-Tools zeigen sie als Ordner an.

**Warum PR-Branches von `master` abzweigen mÃ¼ssen:** Ein PR enthÃ¤lt alle Commits seines Branches, die noch nicht in `upstream/master` sind. Ein Branch von `truely-godot-engine` wÃ¼rde alle eigenen Features mit in den PR ziehen.

## Remotes

- `origin`: der eigene Fork. Hierhin wird gepusht.
- `upstream`: godotengine/godot. Nur fetch, Push ist gesperrt (`DISABLED`).

## TÃ¤glicher Ablauf

```powershell
# Upstream holen und in die eigene Engine mergen
./truely/sync-upstream.ps1 -MergeIntoEngine -Push

# Feature nur fÃ¼r die eigene Engine
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

Wird ein PR in Godot gemergt, kommt er Ã¼ber `master` ohnehin in die eigene Engine. Den PR-Branch kann man danach lÃ¶schen.

## Wichtig

- **PRs auf GitHub:** Vor dem Absenden das *base repository* prÃ¼fen: `TruelyMostWanted/truelymostwanted-godot-engine` fÃ¼r interne PRs, `godotengine/godot` nur fÃ¼r BeitrÃ¤ge. `gh` ist per `gh repo set-default` auf den Fork eingestellt.
- **PR-Branches aktuell halten:** Bei Konflikten mit Godot den PR-Branch per Rebase auf `upstream/master` bringen (`git rebase upstream/master`), dann `git push --force-with-lease`.
- **Versionskennung:** `build.ps1` setzt `BUILD_NAME` und optional `GODOT_VERSION_STATUS` Ã¼ber Umgebungsvariablen. `version.py` bleibt unverÃ¤ndert, damit beim Sync keine Konflikte entstehen.
- **Konflikte beim Sync:** `git rerere` ist aktiv. Ein einmal gelÃ¶ster Konflikt wird beim nÃ¤chsten Merge automatisch wieder so gelÃ¶st.
