# AGENTS.md

Einstieg für KI-Agents (und Menschen), die an dieser Engine-Fork arbeiten.

Dieses Repository ist **truelymostwanted-godot-engine**, eine Fork von [godotengine/godot](https://github.com/godotengine/godot).
Hier entsteht die eigene Engine `truely-godot-engine`: zusätzliche native Engine-Features für Ressourcen, Pfade, Packs, Server und Einstellungen,
nutzbar aus GDScript, C# und GDExtension.

**Jede Aufgabe läuft über ein GitHub-Issue** in diesem Repository. Planung im Chat, Dokumentation im Issue und im Wiki.

## Zuerst lesen

| Datei | Inhalt |
|---|---|
| [`truely/CONVENTIONS.md`](truely/CONVENTIONS.md) | Regeln für Code (Server als C++-Singletons, keine Nodes, Namen, Bindings, Doku) und für Issues (Englisch, Vorlagen, Labels, fünf Abnahmepunkte) |
| [`truely/FEATURE-WORKFLOW.md`](truely/FEATURE-WORKFLOW.md) | Ablauf pro Issue: Feature-Branch, Commits, Mono-Build, Testprojekt, Wiki, Push, CI, Squash-Merge |
| [`truely/README.md`](truely/README.md) | Branches (`master`, `truely-godot-engine`, PR- und Feature-Branches), Remotes, Sync mit Godot, `build.ps1` |
| [`.agents/project.md`](.agents/project.md) | Arbeit mit Agenten (Truely-Run-Agentic-Project): Parameter, Abweichungen vom Skill, Board-IDs, Stand und Übergabe; Fortsetzen mit `Mode=Resume` |
| Tracker-Issue (`flag: tracker`) | Reihenfolge und Abhängigkeiten der laufenden Issues: `gh issue list --label "flag: tracker"` |

## Repository

| Ort | Inhalt |
|---|---|
| `master` | reine Kopie von `upstream/master`, **keine eigenen Commits** |
| `truely-godot-engine` | Standard-Branch: die eigene Engine, wird gebaut |
| `truely/` | eigene Werkzeuge und Doku: `build.ps1`, `sync-upstream.ps1`, `README.md`, `CONVENTIONS.md`, `FEATURE-WORKFLOW.md` |
| `.github/ISSUE_TEMPLATE/` | TRUELY-Vorlagen (Feature, Bug, Refactoring, Concept, Task, …) mit Engine-Bereichen |
| [Wiki](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki) | Doku der Features: Benutzung aus GDScript, C# und GDExtension |
| [godot-custom-build-project](https://github.com/TruelyMostWanted/godot-custom-build-project) | Testprojekt: ein Ordner pro Feature, Checks in GDScript und C# |

- **Remotes:** `origin` = diese Fork (Push), `upstream` = godotengine/godot (nur Fetch, Push gesperrt).
- **Engine-Stand:** Godot 4.8 beta (`version.py`), C++17, ohne Exceptions. C#: .NET 11, `Godot.NET.Sdk/4.8.0-beta` aus dem lokalen NuGet-Ordner.
- **Bauen:** `./truely/build.ps1 module_mono_enabled=yes`, danach Glue und C#-Pakete (`FEATURE-WORKFLOW.md`, Abschnitt 3).

## Arbeitsablauf in Kurzform

1. **Issue** suchen oder anlegen (`gh issue list --state all --search "<Stichwort>"`), nach Vorlage, auf Englisch.
2. **Feature-Branch** von `truely-godot-engine`, Name mit der Issue-Nummer als Präfix (mindestens zweistellig): `03-feature-path-prefix-server`. Beiträge an Godot selbst: PR-Branch von `master`.
3. **Implementieren** nach `CONVENTIONS.md`, **beliebig oft committen**. Der pre-commit-Hook muss durchlaufen.
4. **Mono-Build**, dann im Testprojekt `new-feature.ps1 -Name <feature-branch>` und Checks in GDScript und C# schreiben und ausführen.
5. **Wiki** schreiben, nach dem Aufbau der Godot-Docs: eine Klassenreferenz-Seite pro Klasse (`Class-<Klasse>`) und eine Guide-Seite pro Feature (`<feature-branch>`), Beispiele in GDScript, C# und C++ (GDExtension).
6. **Abnahmepunkte** im Issue abhaken, Ergebnisse als Kommentar ins Issue.
7. **Push** nur bei WIP oder Abschluss → **GitHub Actions abwarten** → bei Grün **Squash-Merge** nach `truely-godot-engine`.
8. **Den Feature-Branch nie löschen** (`gh pr merge --squash` ohne `--delete-branch`).

## Regeln für Agents

- **Öffentlich:** Repository, Wiki und Testprojekt sind öffentlich. Private Repositories werden nirgends erwähnt (keine Namen, Links, Pfade, Klassennamen). Issues begründen sich über „Current state“ und „Proposal“.
- **Sprache:** Unterhaltung mit dem Nutzer auf Deutsch. Issues, Commits, Code-Kommentare und Wiki auf Englisch.
- **Erst prüfen, dann bauen:** den Teil „Feasibility to check“ eines Issues beantworten und im Issue festhalten, bevor die Umsetzung beginnt.
- **Cross-Plattform und Web:** Beim Entwerfen, Planen und Umsetzen jedes Features die Benutzbarkeit und Performance auf Windows, Linux, macOS, Android, iOS **und im Web-Export** bedenken. Features sollen möglichst überall funktionieren, auch im Web (single-threaded, Dateien nur in `user://` und im PCK, wenig Speicher). Was nicht geht, steht im Issue unter „Platforms“.
- **Kein Commit auf `master`.** Keine Änderungen an `version.py` (Konflikte beim Sync).
- **Kein Push zu `upstream`.** PRs an Godot nur auf ausdrücklichen Wunsch, und dann das *base repository* prüfen.
- Pushes starten die volle CI aller Plattformen. Reine Doku- oder Vorlagen-Änderungen dürfen `[skip ci]` in der Commit-Message tragen.
- Commits enden mit der Zeile `Co-Authored-By:` des Agents, wenn ein Agent sie erstellt.
