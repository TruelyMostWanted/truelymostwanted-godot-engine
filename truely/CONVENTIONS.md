# Konventionen der Truely-Engine

Regeln für Code und Issues in dieser Engine-Fork. Der Ablauf (Branch, Build, Test, Merge) steht in [`FEATURE-WORKFLOW.md`](FEATURE-WORKFLOW.md),
Branches und Remotes in [`README.md`](README.md).

---

## 1. Code

### 1.1 Grundregeln

| Regel | Bedeutung |
|---|---|
| **Keine Nodes** | Neue Systeme sind **keine Nodes** im Szenenbaum. Server sind C++-Singletons wie `AudioServer` oder `TextServer`. Datentypen sind `Resource`s. |
| **Das C++-Objekt ist die API** | Methoden, Properties, Signale und Enums werden in `_bind_methods()` gebunden. GDScript sieht sie direkt. C# bekommt sie über den generierten Glue. |
| **Doku gehört dazu** | Jede Klasse hat ihre Klassenreferenz in `doc/classes/<Klasse>.xml` oder `modules/<modul>/doc_classes/`. Die pre-commit-Hooks `doc-status` und `make-rst` müssen durchlaufen. Die Benutzung steht im [Wiki](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki). |
| **Zwei Sprachen** | Jedes Feature wird in **GDScript und C#** getestet, mit einem **Mono-Build** (Testprojekt [godot-custom-build-project](https://github.com/TruelyMostWanted/godot-custom-build-project)). |
| **Cross-Plattform und Web** | Jedes Feature wird für Windows, Linux, macOS, Android, iOS **und den Web-Export** entworfen. Benutzbarkeit und Performance werden pro Plattform bedacht. Der Web-Export ist standardmäßig single-threaded, hat Dateien nur in `user://` (IndexedDB, asynchron gespeichert) und im PCK, keinen Ordner der ausführbaren Datei, keine Prozesse und wenig Speicher. Was pro Plattform geht, langsamer ist oder fehlt, steht im Issue unter „Platforms“. |
| **Stil des Engine-Codes** | Tabs, clang-format, `p_` für Parameter, `r_` für Rückgabeparameter, keine Exceptions (`disable_exceptions=yes`), C++17. |
| **Textformate** | Daten (`Variant` ↔ Text) wie Godots `JSON`: `parse(text) -> Error` (Instanz), `parse_string(text) -> Variant` (statisch), `stringify(data) -> String` (statisch). Ressourcen (`Resource` ↔ Text): `serialize(resource, options = {}) -> String`, `deserialize(text, type_hint = "") -> Resource`, je Formatklasse (`JSON`, `TOML`, `XML`, `INI`, `CSV`, `YAML`) und zentral `ResourceTextSerializer.serialize(resource, format)`/`deserialize(text, format)`. Keine Namen wie `resource_to_x`/`x_to_resource` (#68, #73). |
| **Thread-Sicherheit** | Wo Loader-Threads oder OS-Callbacks beteiligt sind: Locks (`Mutex`, `RWLock`), Signale werden auf dem Main-Thread gesendet (`call_deferred`). **Nie** `ERR_*`, `WARN_*` oder `print_*` aufrufen, während ein Lock gehalten wird: Ausgeben nimmt den globalen Lock (TSAN-Fehler #60); Fehler unter dem Lock sammeln, nach dem Freigeben ausgeben. Kein `ResourceLoader::load()` in einem `WorkerThreadPool`-Task, der auf sich selbst wartet (#9: eigener `Thread`). |
| **Upstream-freundlich** | Eigene Dateien statt großer Änderungen in bestehenden Dateien, wo es geht. Das hält den Sync mit Godot (`sync-upstream.ps1`) konfliktarm. |
| **Ergänzen statt ersetzen** | Langfristig entsteht eine eigene, verbesserte Engine. Bis zu deren echtem Release gilt: **minimal destruction, additive features**. Neue APIs werden unabhängig von Godots Entwurf gebaut (eigene Namen, eigenes Datenmodell) und über Adapter angebunden. Godot-APIs, die sie ablösen, werden in der Klassenreferenz nur als **deprecated** markiert (mit Verweis auf die neue API), nicht entfernt oder umgebaut, z. B. `TranslationServer` → `TextLocalizationServer` (#339). Aktiver Umzug und Overrides im Godot-Kern erst mit dem Release der eigenen Engine-Version. **Ausnahme:** Godots Datenstruktur unter der Haube darf umgebaut werden, wenn die bestehende API dabei erhalten bleibt und nur ergänzt wird (gleiche Methoden und Signale, neue dazu, Datenmodelle teils als `Resource`), und der Umbau einen klaren Gewinn bringt (Performance, Bearbeitbarkeit im Inspector). Vorbild: `InputMap` mit `InputAction`-Ressourcen und Layern (#172, #191). |

### 1.2 Namen

| Wo | Schreibweise | Beispiel |
|---|---|---|
| C++-Klassen | `PascalCase` | `TTSServer`, `TTSPersona` |
| Methoden, Properties, Signale | `snake_case` | `speak()`, `default_persona`, `utterance_started` |
| Booleans | als Property ohne `is_`, Getter `is_*` | `mute`, `borderless` → `is_mute()` |
| Enums | Typ `PascalCase`, Werte `UPPER_SNAKE_CASE` | `UtteranceEvent::UTTERANCE_STARTED` |
| Dateien | `snake_case` | `servers/tts/tts_server.h` |
| Project Settings | `bereich/gruppe/name` in `snake_case` | `filesystem/resources/preload_lists` |
| C# (automatisch) | `PascalCase` | `TtsServer.Speak()`, `DefaultPersona`, `UtteranceStarted` |

### 1.2a Benennung und Typen der API

| Regel | Inhalt | Beispiel |
|---|---|---|
| **Sprechende Namen** | Der Name sagt, was die Methode tut und womit. Keine Kurzformen, keine fremden Wörter („logic“, „select“, „insert“). | `create_folder_prefix()` statt `create_folder_logic()`, `find_resources()` statt `select()` |
| **Godot-Verben** | `add_*`/`remove_*`, `register_*`/`unregister_*`, `has_*`, `get_*`/`set_*`, `is_*`, `find_*`, `get_*_count()`. Paare sind symmetrisch. | `register_noise`/`unregister_noise`, `get_entry_count()` |
| **Rückgabe im Namen** | Liefert eine Methode Namen, IDs oder Pfade, steht das im Namen. | `get_dirty_setting_names()`, `resolve_voice_id()`, `get_file_paths()` |
| **„save“/„load“ nur mit Datei** | `save_*`/`load_*` lesen oder schreiben Dateien. Text in einen String: `to_text()`, aus einem String: `load_from_text()`. | `to_text(FORMAT_JSON)` statt `save_as_json()` |
| **Akronyme** | In Klassennamen groß, wie in Godot (`JSON`, `XMLParser`, `PCKPacker`, `HTTPRequest`). C# macht daraus automatisch `Tts…`, `Pck…`. | `TTSServer`, `PCKPackage` |
| **Engine-Namen übernehmen** | Gibt es das Konzept in Godot schon, denselben Namen verwenden. | `max_polyphony`, `switch_to_clip`, `filter_clip`, `is_bus_mute` |
| **Keine Namenskonflikte** | Kein Klassenname, der einer Engine-Klasse gleicht oder zum Verwechseln ähnlich ist; vorher `grep -rn "class <Name>\b" core scene servers modules editor`. | nicht `TextureAtlas` neben `AtlasTexture` |
| **Enums statt Strings/Bools** | Feste Auswahl = Enum (`PascalCase`, Werte mit Präfix `UPPER_SNAKE_CASE`, `*_MAX` am Ende, Werte nie umnummerieren). Ein Bool-Parameter, dessen Bedeutung am Aufruf nicht lesbar ist, wird ein Enum. Enum-Werte auch in Signal-Argumenten und virtuellen Methoden binden. | `ResourceTextSerializer.Format.FORMAT_JSON`, `resolve_safe(path, ACCESS_WRITE)` |
| **Keine magischen Zahlen** | Sonderwerte wie `-1` bekommen einen Enum-Wert. | `APPLY_PHASE_NONE = -1` |
| **Konstanten vs. Enum-Präfix** | Eine Konstante darf nicht wie ein Wert eines Enums derselben Klasse aussehen. | `TEXT_LAYOUT_VERSION` statt `FORMAT_VERSION` neben `Format` |
| **Typisierte Optionen** | Keine `options: Dictionary`. Optionen sind eine `Resource` mit typisierten Properties (`SerializerOptions`), Parameter mit Default `null` = Werte der Project Settings. Erweiterungen erben von der Options-Klasse statt `extra: Dictionary`. | `serialize(res, FORMAT_TOML, options)` |
| **Typisierte Ergebnisse** | Mehrere Werte als Ergebnis: eigene `RefCounted`-Klasse statt `Dictionary`. Ausnahme: dieselbe Form wie eine bestehende Engine-Methode (`OS.get_memory_info()`, `DisplayServer.tts_get_voices()`). | `FilesManifestValidation`, `PCKPackageEntry` |
| **Typisierte Parameter** | Engine-Typen statt `Resource`/`Variant`, wo möglich (`Noise`, `AudioStream`). `Variant` nur mit dokumentiertem Grund (z. B. `AudioStream` oder Pfad zum späten Laden). Bezeichner und Klassennamen als `StringName`. | `register_noise(name, noise: Noise)`, `type_hint: StringName` |
| **Inspector oder Server** | Was im Inspector bearbeitet und als `.tres` gespeichert wird, ist eine `Resource` (Properties mit Hints: `PROPERTY_HINT_FILE`, `DIR`, `ENUM`, Bereiche). Laufzeitzustand, Registrierungen und Ablauf gehören in den Server (C++-Singleton). Rückgabe-/Hilfsobjekte ohne Speicherung sind `RefCounted`. | `AudioMixerChannel` (Resource) + `AudioMixerServer` |
| **Parameter nicht wie globale Funktionen** | Parameternamen überdecken keine GDScript-Builtins (`load`, `save`, `print`, `range`). | `loader: Callable` statt `load: Callable` |
| **Textformate** | Eingebaute Formate über `ResourceTextSerializer.Format`; Module registrieren ihre Implementierung in C++ für ihren Enum-Wert (`register_format(FORMAT_TOML, …)`), ein fehlendes Modul ergibt `ERR_UNAVAILABLE`. Eigene Formate aus GDExtension/Skript über `ResourceTextFormat` und die `StringName`-API (`register_custom_format`, `serialize_custom`). `resource_to_dictionary()`/`dictionary_to_resource()` sind die Baum-API, kein Format. | — |
| **Unveröffentlichte API** | Umbenennungen ohne Kompatibilitäts-Aliase, solange das Feature nicht veröffentlicht ist. Jede Umbenennung: Doku-XML, Wiki (Klassen- und Guide-Seite), Checks in GDScript und C#. | — |

Prüfliste für neue APIs (vor dem ersten Commit): Namen nach Tabelle, kein Konflikt-`grep`, keine `Dictionary`-Optionen,
keine Strings für feste Auswahlen, Inspector-Hints gesetzt, Signal- und Virtual-Typen gebunden.

### 1.3 Aufbau eines Servers

```cpp
// servers/tts/tts_server.h
class TTSServer : public Object {
	GDCLASS(TTSServer, Object);
	static TTSServer *singleton;

protected:
	static void _bind_methods();

public:
	static TTSServer *get_singleton();
	int speak(const String &p_text, const Ref<TTSPersona> &p_persona);
	// ...
};
```

Registrierung in `register_core_types()` (`core/register_core_types.cpp`), `register_server_types()` (`servers/register_server_types.cpp`)
oder im `register_types.cpp` eines Moduls:

```cpp
GDREGISTER_CLASS(TTSServer);
Engine::get_singleton()->add_singleton(Engine::Singleton("TTSServer", TTSServer::get_singleton(), "TTSServer"));
```

Aus jedem Singleton erzeugt der C#-Generator zwei Typen (`modules/mono/editor/bindings_generator.cpp`, `CS_SINGLETON_INSTANCE_SUFFIX`):
- `TTSServer`: statische Klasse mit der API, zum Beispiel `TTSServer.Speak(...)`.
- `TTSServerInstance`: der Typ des Objekts, für Signale und Referenzen.

### 1.4 Ressourcen

- Datentypen erben von `Resource` und werden mit `GDREGISTER_CLASS` registriert. Damit lassen sie sich im Inspector anlegen und speichern.
- Gespeicherte Properties haben `PROPERTY_USAGE_STORAGE`. Die Namen sind die späteren Schlüssel in JSON, INI und CSV.

### 1.5 Glossar

- Neue Begriffe (Klassen, Server, Workflow-Begriffe) bekommen im selben Issue einen Eintrag in [`GLOSSARY.md`](../GLOSSARY.md) und, wenn es ein Feature-Bereich ist, eine Zeile im Datei-Index.

---

## 2. Branches

- **Feature-Branches tragen als Präfix die Nummer ihres Issues, mindestens zweistellig:** `<nn>-<art>-<kurzname>`, alles klein mit Bindestrichen.
  - Issue #1 → `01-feature-resource-paths-retrieval`
  - Issue #3 → `03-feature-path-prefix-server`
  - Issue #123 → `123-feature-…`
  - `<art>` folgt dem Typ des Issues: `feature`, `refactoring`, `bug`, `concept`.
- Derselbe Name gilt für den Feature-Ordner im Testprojekt (`res://03-feature-path-prefix-server/`) und für die Guide-Seite im Wiki.
- Feature-Branches zweigen von `truely-godot-engine` ab und werden **nie gelöscht**. PR-Branches für Beiträge an Godot zweigen von `master` ab (siehe `README.md`).

---

## 3. Issues

- **Sprache:** Issues, Titel, Commits und Code-Kommentare auf **Englisch**, die Unterhaltung mit dem Nutzer auf Deutsch.
- **Vorlagen:** `.github/ISSUE_TEMPLATE/`. Über die CLI wird der Body als Markdown mit genau den Überschriften der Vorlage geschrieben.

| Art | Vorlage | Titel | Label | Überschriften |
|---|---|---|---|---|
| Neue Funktion | `feature_request.yml` | `[Feature] …` | `type: feature` | Summary, Current state, Proposal, Scope (In/Out of scope), Acceptance criteria, Area, Technical notes, References |
| Umbau | `refactoring.yml` | `[Refactoring] …` | `type: refactoring` | Current state, Target state, Behavior change, Area, Acceptance criteria, References |
| Konzept, Recherche | `concept.yml` | `[Concept] …` | `type: concept` | Summary, Current state, Questions, Approaches, Acceptance criteria, References |
| Fehler | `bug_report.yml` | `[Bug] …` | `type: bug` | siehe Vorlage |
| Organisation, Tracker | `task.yml` | `[Task] …` | `type: task` | Goal, Checklist, Due, Deliverables, References |

- **Öffentlich und für sich verständlich:** Dieses Repository, das Wiki und das Testprojekt sind öffentlich. **Keine Verweise auf private Repositories**: keine Namen, Links, Pfade oder Klassennamen daraus. Ein Issue begründet sich allein über den Stand der Engine („Current state“ mit Code-Stellen) und den Vorschlag („Proposal“).
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
