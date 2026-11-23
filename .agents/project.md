# Truely-Run-Agentic-Project · truelymostwanted-godot-engine

Die eine Datei, mit der dieses Projekt geführt wird: Parameter, Ableitungen, Plattform und der Stand für die nächste Session. `/truely-run-agentic-project Mode=Resume` liest sie und setzt fort; den Rest (Commits seit dem letzten Stand, offene Issues) liest es frisch aus Git und von der Plattform. Bei einem Widerspruch gilt die Plattform. Nur der Project-Agent bzw. der Orchestrator schreibt diese Datei.

**Dieses Projekt ist eine Engine-Fork, kein Spiel.** Der Skill liefert nur die Orchestrierung (Ebenen, Board, Wache, Resume, Kosten, Caveman). Ordnerstruktur, Konventionen und Ablauf kommen aus dem Repository:
- Regeln: [`AGENTS.md`](../AGENTS.md), [`truely/CONVENTIONS.md`](../truely/CONVENTIONS.md), [`truely/FEATURE-WORKFLOW.md`](../truely/FEATURE-WORKFLOW.md). Es gibt **kein** `.agents/CONVENTIONS.md`, kein `godot-project/`, kein `upstream` godot-core-systems.
- Reihenfolge und Abhängigkeiten: Tracker-Issue #32.
- `.agents/state/` (Wissen je Issue, Datei-Index, Kosten) bleibt lokal (`.gitignore`); nur diese Datei ist versioniert. Das Repository ist öffentlich.

## Abweichungen vom Skill (Besitzer, 2026-11-23)

| Skill | Hier |
|---|---|
| Branch `<nr>-<name>` von `main` | Branch `<nn>-feature\|refactoring\|bug\|concept-<name>` von `truely-godot-engine`, Nummer mindestens zweistellig |
| Project-Agent mergt lokal mit `--no-ff` und löscht den Branch | Project-Agent pusht den Feature-Branch **bei Abschluss**, wartet auf **grüne GitHub Actions**, dann PR nach `truely-godot-engine` und `gh pr merge --squash` **ohne `--delete-branch`**. **Feature-Branches werden nie gelöscht**, weder auf GitHub noch lokal. Nur der Worktree wird entfernt. |
| Issue-Agent committet im Branch | gleich: **beliebig oft** committen, nie pushen; der pre-commit-Hook muss durchlaufen |
| `.agents/CONVENTIONS.md`, C# 15, `core/` von godot-core-systems | `truely/CONVENTIONS.md`: C++17-Server als Singletons, Resources, Bindings, `doc/classes/`, keine Nodes; die Godot-Quellen dürfen geändert werden (upstream-freundlich, eigene Dateien bevorzugt) |
| Issues aus Goals anlegen | Issues #1–#31 existieren schon; neue Funde als eigenes Issue nach `.github/ISSUE_TEMPLATE/` |
| Concept-Issues setzen um | Concept-Issues (#4, #6, #13, #14, #31) liefern einen Vorschlag als Kommentar; **entscheidet der Besitzer** |
| Abnahme: Akzeptanzkriterien | dazu die fünf festen Abnahmepunkte (C++ API, Bindings, Doku inkl. Wiki, Test-Build, GDScript- und C#-Test im Testprojekt) |

## Parameter

Bei `Start` geschrieben; danach nur über „setze X = Y“, mit Datum unter „Änderungen“.

```ini
MODE='Start'
GOALS='existing GitHub issues #1-#31, order and dependencies from tracker #32'
CONVENTIONS='AGENTS.md + truely/CONVENTIONS.md + truely/FEATURE-WORKFLOW.md'
HOST_PARAMETER=''
REPO_PARAMETER='TruelyMostWanted/truelymostwanted-godot-engine'
APPROVAL='Autonomous'
CHECKPOINTS=''
MAX_ISSUE_AGENTS='1'
MAX_ISSUES_PER_AGENT='1'
ALLOW_TEAMS_PER_ISSUE='false'
MIRROR_GIT_ISSUES='false'
USE_CAVEMAN='true'
```

Änderungen:
- 2026-11-23: angelegt (Besitzer: Ordnerstruktur des Engine-Repositorys, Squash-Merge nach grüner CI, Feature-Branches bleiben bestehen, gemeinsamer SCons-Cache, Board mit Skill-Status und Phasen, `.agents/state/` lokal)
- 2026-11-23: Besitzer: Entscheidungen (auch Concept-Issues) trifft der Orchestrator selbstständig und vorausschauend; Entscheidung als Kommentar „Decision“ ins Issue. APPROVAL=Autonomous.
- 2026-11-23: Besitzer: **Wiki-Seiten sind immer Pflicht**, für jedes umgesetzte Issue (auch Refactorings ohne neue API) Guide-Seite und Klassenreferenz jeder betroffenen Klasse; Concept-Entscheidungen auf der Seite `Design-Decisions`. Jede Seite mit nummeriertem Inhaltsverzeichnis (`python truely/wiki-toc.py <wiki-ordner>` vor jedem Wiki-Push). Der Orchestrator veröffentlicht sie sofort nach dem Bericht des Agenten (Status „in review“ auf `Home`), nach dem Merge Status „done“.
- 2026-11-23: Besitzer: **Testcode vorab:** Für jedes Feature schreibt ein (günstiger) Agent die GDScript- und C#-Checks im Testprojekt schon vor dem Engine-Build, auf dem Feature-Branch gleichen Namens, und pusht ihn. Nach `main` des Testprojekts erst nach dem Merge in `truely-godot-engine` und dem Engine-Build; dann dort testen.
- 2026-11-23: Besitzer: **CI in Stapeln (#46):** keine CI auf Feature-Branches und PRs; Merge, sobald lokal grün; volle CI nur auf `truely-godot-engine`, etwa ein Lauf je fünf Features; rot → Bug-Issue und Fix-Branch.
- 2026-11-23 (ca. 13:35): Besitzer: **Wochenlimit 97 %: keine neuen Agenten.** Die laufenden (#11, #14) zu Ende laufen lassen. Neue Agenten erst nach Freigabe des Besitzers.
- 2026-11-23: Besitzer: **Features parallel** auf eigenen Branches; Auswahl nach getrennten Dateien (gemeinsame Registrierungsdateien nur mit wenigen Zeilen an anderer Stelle), weil `truely-godot-engine` nie in Feature-Branches gemergt wird. Weiter höchstens 2 Agenten (Wochenlimit), höchstens ein Engine-Build gleichzeitig.
- 2026-11-23: Besitzer: Wochenlimit bei 95 %. Höchstens 2 Agenten gleichzeitig, bevorzugt ein Issue nach dem anderen in der Reihenfolge von Tracker #32. MAX_ISSUE_AGENTS=1, MAX_ISSUES_PER_AGENT=1.

## Abgeleitet

```ini
PROJECT_NAME='truelymostwanted-godot-engine'
LOCAL_PATH='P:/Projects/GitHub/truelymostwanted-godot-engine'
BASE_BRANCH='truely-godot-engine'
TECH='Godot 4.8 beta engine fork, C++17 without exceptions, SCons; C# glue .NET 11, Godot.NET.Sdk 4.8.0-beta from local NuGet'
BUILD_COMMAND='powershell -NoProfile -ExecutionPolicy Bypass -File truely/build.ps1 module_mono_enabled=yes accesskit=no d3d12=no cache_path=P:/scons-cache cache_limit=40'  # tests: add tests=yes
GLUE_COMMAND='./bin/godot.windows.editor.x86_64.mono.exe --headless --generate-mono-glue modules/mono/glue'
ASSEMBLIES_COMMAND='python modules/mono/build_scripts/build_assemblies.py --godot-output-dir=./bin --godot-platform=windows --push-nupkgs-local P:/Projects/GitHub/nuget-local'
TEST_PROJECT='P:/Projects/GitHub/godot-custom-build-project'
TEST_COMMAND='./godot-project/checks/run-checks.ps1 -Godot <worktree>/bin/godot.windows.editor.x86_64.mono.exe -Feature <feature-branch>'
SCONS_CACHE='P:/scons-cache (env TRUELY_SCONS_CACHE, limit 40 GiB)'
MAX_PARALLEL_ENGINE_BUILDS='1'
PROCESS_PATTERN='scons*|godot*|cl.exe|link.exe'
UPSTREAM='https://github.com/godotengine/godot.git (fetch only, push disabled)'
WIKI='https://github.com/TruelyMostWanted/truelymostwanted-godot-engine.wiki.git'
TEST_BUDGET='300s (engine build excluded)'
PROCESS_LIMIT='90m for scons, 8m otherwise'
OBSERVATION_INTERVAL='5m'
MAX_CONTEXT_SIZE='35%'
MAX_AGENT_STEPS='100'
MAX_PARALLELISM_LIMIT='85%'
ISSUE_AGENT_MODEL='opus-medium'
TRACK_AGENT_COSTS='true'
LANGUAGE='de'
ISSUE_LANGUAGE='en'
```

## Plattform

```ini
PLATFORM='github'
HOST='github.com'
OWNER='TruelyMostWanted'
REPO='TruelyMostWanted/truelymostwanted-godot-engine'
PROJECT_NUMBER='5'
PROJECT_ID='PVT_kwHOArNCm84Bsymv'
BOARD_NAME='Truely Godot Engine'
BOARD_URL='https://github.com/users/TruelyMostWanted/projects/5'
STATUS_FIELD_ID='PVTSSF_lAHOArNCm84Bsymvzhq_C1Q'
OPT_BACKLOG='f26ba37e'
OPT_PLANNING='60aa0e0c'
OPT_NEEDS_BUGFIX='d5b733fb'
OPT_WORK_IN_PROGRESS='c87261e6'
OPT_REVIEW='dba19b8b'
OPT_COMPLETED='2663aaea'
```

## Stand

```ini
LAST_INTEGRATED_COMMIT='6a30858649'
CURRENT_MILESTONE='Phase 1 · Resource I/O foundation'
UPDATED='2026-11-23'
```

**In Arbeit:** –

## Übergabe

**Zuerst fortsetzen:** Stapel 1 gemergt (#38, #46, #1, #2, #3, #39, #11); CI-Lauf auf `truely-godot-engine` prüfen; Testprojekt-Branches 01, 02, 03, 39, 11 nach `main` des Testprojekts und dort testen; Wiki-Status „done“. Danach je ein Agent (nach Freigabe, Wochenlimit): #37 → #34 → #5 → #7 → #35 → #36 → #8 → #9 → #10; Checks vorab liegen für #37 und #34 im Testprojekt.

**Entschieden:** #4 und #6 (Kommentare „Decision“ in den Issues): #2 mit Copy-on-write-Snapshot, #34 Endungs-Index darin, #5 JSON/INI/CSV im Core plus Plain-INI-Modus und XML-Writer, `.json` über Typ-Marker mit Durchfallen zum alten Loader, TOML als Modul (#36), YAML vorerst nicht.

**Anhänge-Dateien:** `core/io/resource_loader.*`, `resource_saver.*`, `core/core_bind.*`, `core/register_core_types.cpp`: nie zwei Issues parallel darauf.

**Fragen an den Besitzer:** –

**Am Bildschirm zu prüfen (manual):** –

**Regeln, neu:** –

**Fallen:**
- Auf diesem Rechner fehlen die Abhängigkeiten für AccessKit und D3D12: lokal immer `accesskit=no d3d12=no` (die CI baut mit allem). Die SCons-Meldung „ANGLE deps missing“ ist keine Warnung.
- `build.ps1` hatte positionelle Parameter (`module_mono_enabled=yes` landete in `-Target`); seit 2026-11-23 behoben.
- Agenten in Worktrees dürfen nicht nach `.agents/state/` im Hauptbaum schreiben: `knowledge.md` im Scratchpad unter `issues/<N>/`, der Orchestrator kopiert sie.
- Testprojekt: Branches brauchen `git -c core.longpaths=true`; `nuget-local` enthält immer die Pakete des zuletzt gebauten Branches, vor C#-Checks auf dem eigenen Branch neu bauen.
- `P:/Projects/GitHub/nuget-local` fehlte am 2026-11-23: der erste volle Mono-Build (Glue + Assemblies) legt ihn an.
- Der vorhandene Build in `bin/` ist ein `dev`-Build ohne SCons-Cache; der erste Build mit Cache füllt `P:/scons-cache`.
- Alter Branch `origin/feature-resource-paths-retrieval` (ohne Nummer) ist ein Vorläufer von `01-feature-resource-paths-retrieval`; nicht löschen.
- Auf P: waren am 2026-11-23 nur 115 GB frei; höchstens ein Engine-Build gleichzeitig.

**Wache und Eingriffe:** –
