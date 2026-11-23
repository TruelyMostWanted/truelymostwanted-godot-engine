# Konventionen der Truely-Engine

Regeln für Code und Issues in dieser Engine-Fork. Der Ablauf (Branch, Build, Test, Merge) steht in [`FEATURE-WORKFLOW.md`](FEATURE-WORKFLOW.md),
Branches und Remotes in [`README.md`](README.md).

---

## 1. Code

### 1.1 Grundregeln

| Regel | Bedeutung |
|---|---|
| **Keine Nodes** | Unsere C#-Systeme (godot-file-system, godot-core-systems) sind Nodes im Szenenbaum. **In der Engine nicht.** Server sind C++-Singletons wie `AudioServer` oder `TextServer`. Datentypen sind `Resource`s. |
| **Das C++-Objekt ist die API** | Methoden, Properties, Signale und Enums werden in `_bind_methods()` gebunden. GDScript sieht sie direkt. C# bekommt sie über den generierten Glue. |
| **Doku gehört dazu** | Jede Klasse hat ihre Klassenreferenz in `doc/classes/<Klasse>.xml` oder `modules/<modul>/doc_classes/`. Die pre-commit-Hooks `doc-status` und `make-rst` müssen durchlaufen. Die Benutzung steht im [Wiki](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki). |
| **Zwei Sprachen** | Jedes Feature wird in **GDScript und C#** getestet, mit einem **Mono-Build** (Testprojekt [godot-custom-build-project](https://github.com/TruelyMostWanted/godot-custom-build-project)). |
| **Cross-Plattform und Web** | Jedes Feature wird für Windows, Linux, macOS, Android, iOS **und den Web-Export** entworfen. Benutzbarkeit und Performance werden pro Plattform bedacht. Der Web-Export ist standardmäßig single-threaded, hat Dateien nur in `user://` (IndexedDB, asynchron gespeichert) und im PCK, keinen Ordner der ausführbaren Datei, keine Prozesse und wenig Speicher. Was pro Plattform geht, langsamer ist oder fehlt, steht im Issue unter „Platforms“. |
| **Stil des Engine-Codes** | Tabs, clang-format, `p_` für Parameter, `r_` für Rückgabeparameter, keine Exceptions (`disable_exceptions=yes`), C++17. |
| **Thread-Sicherheit** | Wo Loader-Threads oder OS-Callbacks beteiligt sind: Locks (`Mutex`, `RWLock`), Signale werden auf dem Main-Thread gesendet (`call_deferred`). |
| **Upstream-freundlich** | Eigene Dateien statt großer Änderungen in bestehenden Dateien, wo es geht. Das hält den Sync mit Godot (`sync-upstream.ps1`) konfliktarm. |

### 1.2 Namen

| Wo | Schreibweise | Beispiel |
|---|---|---|
| C++-Klassen | `PascalCase` | `TtsServer`, `TtsPersona` |
| Methoden, Properties, Signale | `snake_case` | `speak()`, `default_persona`, `utterance_started` |
| Booleans | als Property ohne `is_`, Getter `is_*` | `mute`, `borderless` → `is_mute()` |
| Enums | Typ `PascalCase`, Werte `UPPER_SNAKE_CASE` | `UtteranceEvent::UTTERANCE_STARTED` |
| Dateien | `snake_case` | `servers/tts/tts_server.h` |
| Project Settings | `bereich/gruppe/name` in `snake_case` | `filesystem/resources/preload_lists` |
| C# (automatisch) | `PascalCase` | `TtsServer.Speak()`, `DefaultPersona`, `UtteranceStarted` |

### 1.3 Aufbau eines Servers

```cpp
// servers/tts/tts_server.h
class TtsServer : public Object {
	GDCLASS(TtsServer, Object);
	static TtsServer *singleton;

protected:
	static void _bind_methods();

public:
	static TtsServer *get_singleton();
	int speak(const String &p_text, const Ref<TtsPersona> &p_persona);
	// ...
};
```

Registrierung in `register_core_types()` (`core/register_core_types.cpp`), `register_server_types()` (`servers/register_server_types.cpp`)
oder im `register_types.cpp` eines Moduls:

```cpp
GDREGISTER_CLASS(TtsServer);
Engine::get_singleton()->add_singleton(Engine::Singleton("TtsServer", TtsServer::get_singleton(), "TtsServer"));
```

Aus jedem Singleton erzeugt der C#-Generator zwei Typen (`modules/mono/editor/bindings_generator.cpp`, `CS_SINGLETON_INSTANCE_SUFFIX`):
- `TtsServer`: statische Klasse mit der API, zum Beispiel `TtsServer.Speak(...)`.
- `TtsServerInstance`: der Typ des Objekts, für Signale und Referenzen.

### 1.4 Ressourcen

- Datentypen erben von `Resource` und werden mit `GDREGISTER_CLASS` registriert. Damit lassen sie sich im Inspector anlegen und speichern.
- Gespeicherte Properties haben `PROPERTY_USAGE_STORAGE`. Die Namen sind die späteren Schlüssel in JSON, INI und CSV.

---

## 2. Issues

- **Sprache:** Issues, Titel, Commits und Code-Kommentare auf **Englisch**, die Unterhaltung mit dem Nutzer auf Deutsch.
- **Vorlagen:** `.github/ISSUE_TEMPLATE/`. Über die CLI wird der Body als Markdown mit genau den Überschriften der Vorlage geschrieben.

| Art | Vorlage | Titel | Label | Überschriften |
|---|---|---|---|---|
| Neue Funktion | `feature_request.yml` | `[Feature] …` | `type: feature` | Summary, Motivation, Scope (In/Out of scope), Acceptance criteria, Area, Technical notes, References |
| Umbau | `refactoring.yml` | `[Refactoring] …` | `type: refactoring` | Current state, Target state, Behavior change, Area, Acceptance criteria, References |
| Konzept, Recherche | `concept.yml` | `[Concept] …` | `type: concept` | Summary, Motivation, Questions, Approaches, Acceptance criteria, References |
| Fehler | `bug_report.yml` | `[Bug] …` | `type: bug` | siehe Vorlage |
| Organisation, Tracker | `task.yml` | `[Task] …` | `type: task` | Goal, Checklist, Due, Deliverables, References |

- **Labels:** genau ein `type:`, mindestens ein `area:` (Engine-Bereiche: `core`, `scene`, `servers`, `editor`, `modules`, `dotnet`, `platform`, `thirdparty`, `build`, `tests`, `docs`), 0–3 `topic:`.
- **Machbarkeit:** Jedes Feature-Issue hat unter „Technical notes“ einen Teil **„Feasibility to check“**: was in der Engine geht und was nicht. Das Ergebnis wird im Issue festgehalten, bevor die Umsetzung beginnt.
- **Abnahme:** Jedes Feature- und Refactoring-Issue endet mit diesen fünf Punkten:

```markdown
- [ ] C++ data structures and API created
- [ ] C#/GDScript bindings created and registered
- [ ] Documentation written (class reference in `doc/classes/` and wiki page: GDScript, C#, GDExtension)
- [ ] Test build on the current platform done (no errors or warnings)
- [ ] Tested in the test project (`res://<feature-branch>/`) with a Mono build, in GDScript and in C#
```

- **Wiki:** Klassenreferenz-Seiten (`Class-<Klasse>`) und Guide-Seiten (`<feature-branch>`) nach dem Aufbau der offiziellen Godot-Docs, mit Beispielen in GDScript, C# und C++ (GDExtension). Details: `FEATURE-WORKFLOW.md`, Abschnitt 5.
- **Tracker:** Größere Vorhaben bekommen ein `[Task] Tracker: …` mit `flag: tracker`, das die Issues in Reihenfolge und mit Abhängigkeiten auflistet.
