# Truely-Run-Agentic-Project · truelymostwanted-godot-engine

Die eine Datei, mit der dieses Projekt geführt wird: Parameter, Ableitungen, Plattform und der Stand für die nächste Session. `/truely-run-agentic-project Mode=Resume` liest sie und setzt fort; den Rest (Commits seit dem letzten Stand, offene Issues) liest es frisch aus Git und von der Plattform. Bei einem Widerspruch gilt die Plattform. Nur der Project-Agent bzw. der Orchestrator schreibt diese Datei.

**Dieses Projekt ist eine Engine-Fork, kein Spiel.** Der Skill liefert nur die Orchestrierung (Ebenen, Board, Wache, Resume, Kosten, Caveman). Ordnerstruktur, Konventionen und Ablauf kommen aus dem Repository:
- Regeln: [`AGENTS.md`](../AGENTS.md), [`truely/CONVENTIONS.md`](../truely/CONVENTIONS.md), [`truely/FEATURE-WORKFLOW.md`](../truely/FEATURE-WORKFLOW.md). Es gibt **kein** `.agents/CONVENTIONS.md`, kein `godot-project/`, kein `upstream` godot-core-systems.
- Reihenfolge und Abhängigkeiten: Tracker-Issue #32.
- `.agents/state/` (Wissen je Issue, Datei-Index, Kosten) bleibt lokal (`.gitignore`); nur diese Datei ist versioniert. Das Repository ist öffentlich.
- Eine lokale Kopie des Skills liegt in `.agents/skills/truely-run-agentic-project/` (gitignored, weil sie private Repositories nennt). Neu kopieren mit `cp -r ~/.claude/skills/truely-run-agentic-project .agents/skills/`.

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
MAX_ISSUE_AGENTS='5'
MAX_ISSUES_PER_AGENT='1'
ALLOW_TEAMS_PER_ISSUE='false'
MIRROR_GIT_ISSUES='false'
USE_CAVEMAN='true'
```

Änderungen:
- 2026-11-24 (ca. 01:30): Besitzer, **voll autonom ohne Rückfragen**: alle Issues (Konzept, Design, Code, Wiki, Tests, Merge) selbst abarbeiten. Vorgaben: (1) Namen von Features/APIs aussagekräftig machen, Parameter und Typen nutzen (Enums statt Strings, typisierte Options-Klassen statt Dictionary, z. B. `ResourceTextSerializer`); was im Inspector bearbeitbar sein soll → `Resource`, Rest Backend/Server. (2) **Wiki zuerst**: Guides kurz, direkt, verständlich; Home/Sidebar nach Themen gruppiert (Text-Serialisierung, Settings, File I/O, Laden & Speichern von Ressourcen, Packs, Audio, Performance …), Klassenreferenz alphabetisch. (3) Neu: `HttpRequestTarget`-Resource für `HTTPRequest`; Input-Settings (#19) einfach für Spiele; Laufzeit-Loader/-Saver für typische Assets (Bilder, Audio, Video …), **je Ressource ein eigenes Issue und Branch**. (4) Session-Limit: auf automatische Fortsetzung warten. Caveman, sparsam.
- 2026-11-24: 5-Stunden-Limit bei 95 %: keine neuen Agenten; laufende mit WIP gestoppt.
- 2026-11-24: Besitzer: **Änderungen an bestehenden Features auf deren Branch** (nicht nur Bugs: Umbenennungen, API-Anpassungen, Doku-/Testfixes), Engine und Testprojekt, danach erneut squash-mergen. Neue Issues mit neuen Klassen behalten ihren Branch.
- 2026-11-24: Besitzer: 3–5 Issues parallel (Wochen-/Session-Limit zurückgesetzt). MAX_ISSUE_AGENTS=5.
- 2026-11-23: angelegt (Besitzer: Ordnerstruktur des Engine-Repositorys, Squash-Merge nach grüner CI, Feature-Branches bleiben bestehen, gemeinsamer SCons-Cache, Board mit Skill-Status und Phasen, `.agents/state/` lokal)
- 2026-11-23: Besitzer: Entscheidungen (auch Concept-Issues) trifft der Orchestrator selbstständig und vorausschauend; Entscheidung als Kommentar „Decision“ ins Issue. APPROVAL=Autonomous.
- 2026-11-23: Besitzer: **Wiki-Seiten sind immer Pflicht**, für jedes umgesetzte Issue (auch Refactorings ohne neue API) Guide-Seite und Klassenreferenz jeder betroffenen Klasse; Concept-Entscheidungen auf der Seite `Design-Decisions`. Jede Seite mit nummeriertem Inhaltsverzeichnis (`python truely/wiki-toc.py <wiki-ordner>` vor jedem Wiki-Push). Der Orchestrator veröffentlicht sie sofort nach dem Bericht des Agenten (Status „in review“ auf `Home`), nach dem Merge Status „done“.
- 2026-11-23: Besitzer: **Testcode vorab:** Für jedes Feature schreibt ein (günstiger) Agent die GDScript- und C#-Checks im Testprojekt schon vor dem Engine-Build, auf dem Feature-Branch gleichen Namens, und pusht ihn. Nach `main` des Testprojekts erst nach dem Merge in `truely-godot-engine` und dem Engine-Build; dann dort testen.
- 2026-11-23: Besitzer: **Bugfixes eines Features auf seinem Feature-Branch**, dann denselben Branch erneut squash-mergen; keine eigenen Fix-Branches für Feature-Fehler. Vor dem Merge `--doctool` auf dem Branch prüfen.
- 2026-11-23: Besitzer: **CI in Stapeln (#46):** keine CI auf Feature-Branches und PRs; Merge, sobald lokal grün; volle CI nur auf `truely-godot-engine`, etwa ein Lauf je fünf Features; rot → Bug-Issue und Fix-Branch.
- 2026-11-23 (ca. 13:35): Besitzer: **Wochenlimit 97 %: keine neuen Agenten.** Die laufenden (#11, #14) zu Ende laufen lassen. Neue Agenten erst nach Freigabe des Besitzers.
- 2026-11-23: Besitzer: **Features parallel** auf eigenen Branches; Auswahl nach getrennten Dateien (gemeinsame Registrierungsdateien nur mit wenigen Zeilen an anderer Stelle), weil `truely-godot-engine` nie in Feature-Branches gemergt wird. Weiter höchstens 2 Agenten (Wochenlimit), höchstens ein Engine-Build gleichzeitig.
- 2026-11-23: Besitzer: Wochenlimit bei 95 %. Höchstens 2 Agenten gleichzeitig, bevorzugt ein Issue nach dem anderen in der Reihenfolge von Tracker #32. MAX_ISSUE_AGENTS=1, MAX_ISSUES_PER_AGENT=1.
- 2026-11-23 (Abend): Besitzer: Resume mit **Decision=FullAutonomous** und **UseCaveman=true** (Chat knapp, ohne Füllwörter). Erst selbst nacheinander (#37, #34), dann: **3 Issues parallel** mit Agenten auf Feature-Branches; Agenten implementieren und schreiben den Testcode im Testprojekt-Branch, **ohne Engine-Build**; Build, Test und Merge macht der Orchestrator danach nacheinander. MAX_ISSUE_AGENTS=3.

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
LAST_INTEGRATED_COMMIT='30ad6d8ff9'
CURRENT_MILESTONE='Resource formats, settings domains, API review'
UPDATED='2026-11-24'
```

Gemergt (2026-11-24, nach dem Limit-Reset): #87, #17, #70, #83, #89, #78, #51 (+ Laufzeit-Teil auf Branch 50), #100, #101, #102, #103, #104, #105, #95, #96, #19. Wiki neu nach Themen (#98 erledigt), alle Seiten dieser Features eingepflegt (letzter Batch #96/#19 läuft per Agent).

**Stand (2026-11-24, Mittag):** Gemergt seit #99: #10/#15–#20-Umbenennungen, #90, #26 (+Testfix), #73, #155 YAML, #85, #122, #86 (+PCKBuildPlan typisiert, im Core registriert), #21, #79, #71, #89/#91 Pack-Modus, #22. Wiki aktuell bis #91 (Batch #22 läuft).
**Fallen neu:** Glue-Generator löscht alte generierte Dateien nicht (Klasse zwischen Core/Editor verschoben → alte Datei in `modules/mono/glue/GodotSharp/*/Generated` löschen, Quelle `touch`en, Assemblies neu, im Test-Worktree `.godot/mono/temp` löschen). Datenklassen, die C# zur Laufzeit braucht, im Core registrieren. `integrate.sh` bricht bei roten Unit-Tests ab (ein Merge von #26 ging vorher mit roten Tests durch, korrigiert).
**Stand (2026-11-24, Nacht):** Gemergt: #184, #185 (+ Mischpult unter Klassen-Header), #189 Float-Rundung, Inspector-Panels unter Header (#85/#51). Wiki: alle Settings-Seiten nach Audio-Muster, Rauschen bereinigt. **Offen, Build-Branch `191-feature-input-action-rebinder`** (enthält #193 von Branch 18 + #191; baut, Core-Textnamen vereinheitlicht: Keyboard/KeyboardLabel/KeyboardPrinted/Mouse/Gamepad/GamepadAxis, alte Namen werden gelesen): 3 rote Unit-Tests in tests/servers/test_input_map_settings_server.cpp aus #193 (nie gebaut): (1) Zeile ~422 Gamepad-Achse mit fester DeviceId wirkt auch auf Gerät 0, (2) ~460 save_mode ALL schreibt keine BuiltIn/Custom-Layer bzw. ui_accept, (3) ~520 v2-INI lädt nicht ("contains InputMapSettingsActions, but InputMapSettingsRows was expected": Typ-Hint für v2). Plus Text-Konvention `[Device<N>:]<Type>:[<Modifiers>+]<Input>` umsetzen (Gamepad-Namen nur in Textform: LeftStickLeft/…/RightTrigger, LeftStickPress; Godot-Enums unverändert; Feld Direction entfällt), mit Tests und Doku-Beispielen (Kommentare im Issue #193). Danach: Checks 18/172/191 im Testprojekt (Textnamen!), merge (Closes #193, #191), action_events_changed erst nach dem Anwenden feuern lassen (Doku sagt heute "before applied"), Wiki Input-Map-Settings (Rebind-Knopf, Dateiformat B). Branch 18 kompiliert allein nicht (Deklarationen fehlen) – nur 191 bauen.
**Alt (überholt):** #93 (SerializerOptions + Format-Enum, Branch `05-feature-resource-text-formats`, inkl. Aufrufer und Test-Branches), #88 (`88-feature-streaming-assets-server`), #84 (Branch `10-feature-pck-package-server`), #75 (`75-feature-audio-commands`, ggf. Mini-Commit auf `74-…`).

**Werkzeuge:** `scratch/integrate.sh <N> <engine-branch> <test-branch> <feature-folder> [all|build|checks]` (Merge, Build, Tests, Glue, Assemblies, doctool, Checks; nur Summen) und `scratch/merge.sh` (Kommentar, PR, Squash-Merge, Testprojekt nach main, Board). Wiki: Clone `scratch/issues/98/wiki`, Einpflegen per Sonnet-Agent mit `scratch/brief-wiki-integrate.md`. Brief für Issue-Agenten: `scratch/brief-common.md`.

**Achtung Stash:** `git stash list` hat `stash@{1}` „new feature for resoure-loader“ (vom Besitzer, alt, auf master) und `stash@{0}` „ACCIDENTAL …“ (versehentlich angewendete Kopie davon, mit Konfliktmarkern). Nichts verworfen; die Skripte nutzen keinen Stash mehr. Besitzer entscheidet über das Löschen.

**Danach:** #99 restliche Umbenennungen je Feature-Branch (Reihenfolge in `scratch/issues/99/review.md`), #73 Formatklassen (nach #93), #85/#86 (PCK-Inspector/Exporter), #90/#91 (Apple-Bundles, Pack-Modus), #76, #79/#80, #71 (Branch 15), #121 (Import-Absturz), #122 (Bus-Layout, Branch 16), Settings-Domänen #18, #20–#30.

## Übergabe

**Warten auf den Besitzer:** Concepts #65 (PCK als Ressource + Editor-Importer/Extract) und #66 (Streaming Assets): Empfehlungen im Issue, Umsetzung erst nach Zustimmung.

**Entschieden:** #4 und #6 (Kommentare „Decision“ in den Issues): #2 mit Copy-on-write-Snapshot, #34 Endungs-Index darin (umgesetzt; Skript-/Binary-Loader in #53), #5 JSON/INI/CSV im Core plus Plain-INI-Modus und XML-Writer, `.json` über Typ-Marker mit Durchfallen zum alten Loader, TOML als Modul (#36), YAML vorerst nicht.

**Anhänge-Dateien:** `core/io/resource_loader.*`, `resource_saver.*`, `core/core_bind.*`, `core/register_core_types.cpp`: parallel nur mit wenigen Zeilen an anderer Stelle.

**Fragen an den Besitzer:** –

**Am Bildschirm zu prüfen (manual):** #10: Check „Haupt-PCK eines exportierten Builds“ nur im Export. #8: Export-Build mit `user://`/`exec://`-Ordnern, `res://` schreibgeschützt; danach #8 schließen. #12: `TtsServer.speak()` mit echten Stimmen unter Windows (GDScript und C#) und Fallback auf eine Stimme gleicher Sprache; dazu ohne `--headless` mit `audio/general/text_to_speech` an (Checks im Testprojekt `res://12-feature-tts-server/` machen das automatisch, wenn TTS verfügbar ist). Danach #12 schließen.

**Regeln, neu:** –

**Fallen:**
- Testprojekt-Worktrees brauchen die Junction `scratch/issues/<N>/nuget-local` → sonst „Godot.NET.Sdk nicht gefunden“.
- `.tres` sortiert Dictionary-Schlüssel: Reihenfolge separat speichern (#50 `region_order`).
- doctest `const Array a = { { Variant(x) } }` wird flach: `push_back(Array({ ... }))` verwenden (#15).
- Erster `--import` des Testprojekts nach dem Merge von #5 stürzte einmal ab (Access Violation, Exit -1073741819), danach nicht reproduzierbar. Beobachten.
- Agent-Worktrees belegen ihren Branch: vor `git switch <branch>` im Hauptbaum `git -C .claude/worktrees/<agent> switch --detach`. Testprojekt-Worktrees im Scratch brauchen die Junction `scratch/issues/<N>/nuget-local` → `P:/Projects/GitHub/nuget-local` (relativer Pfad in `nuget.config`).
- **`run-checks.ps1` mit der GUI-Exe meldete immer PASSED** (PowerShell wartet nicht, kein Exit-Code; GD und C# liefen gleichzeitig). Seit Testprojekt `95830a0` nimmt das Skript die `.console.exe` daneben; Branches ohne diesen Fix: `-Godot …mono.console.exe` direkt übergeben. Stapel 1 wurde damit erneut geprüft: grün.
- C#-Checks: den von `GetCachedRef()` u. Ä. gelieferten Wrapper nicht `using`/`Dispose()` – er ist mit gehaltenen Referenzen geteilt und gibt die Ressource frei.
- Batch-CI 1 scheiterte nur an `--doctool`: `>` in Codebeispielen der XML muss `&gt;` sein (#48). Vor jedem Stapel `--doctool` auf `truely-godot-engine` prüfen; Agenten-Aussagen „kein Diff“ beziehen sich nur auf ihren Branch.
- Auf diesem Rechner fehlen die Abhängigkeiten für AccessKit und D3D12: lokal immer `accesskit=no d3d12=no` (die CI baut mit allem). Die SCons-Meldung „ANGLE deps missing“ ist keine Warnung.
- `build.ps1` hatte positionelle Parameter (`module_mono_enabled=yes` landete in `-Target`); seit 2026-11-23 behoben.
- Agenten in Worktrees dürfen nicht nach `.agents/state/` im Hauptbaum schreiben: `knowledge.md` im Scratchpad unter `issues/<N>/`, der Orchestrator kopiert sie.
- Testprojekt: Branches brauchen `git -c core.longpaths=true`; `nuget-local` enthält immer die Pakete des zuletzt gebauten Branches, vor C#-Checks auf dem eigenen Branch neu bauen.
- `P:/Projects/GitHub/nuget-local` fehlte am 2026-11-23: der erste volle Mono-Build (Glue + Assemblies) legt ihn an.
- Der vorhandene Build in `bin/` ist ein `dev`-Build ohne SCons-Cache; der erste Build mit Cache füllt `P:/scons-cache`.
- Alter Branch `origin/feature-resource-paths-retrieval` (ohne Nummer) ist ein Vorläufer von `01-feature-resource-paths-retrieval`; nicht löschen.
- Auf P: waren am 2026-11-23 nur 115 GB frei; höchstens ein Engine-Build gleichzeitig.

**Wache und Eingriffe:** –
