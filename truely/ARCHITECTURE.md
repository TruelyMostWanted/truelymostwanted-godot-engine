# Architektur der Truely-Engine

Wie Features in dieser Fork aufgebaut werden. Die Regeln sind aus der Architektur der Godot Engine abgeleitet; jede Regel
nennt ihr Vorbild in Godot und zeigt ein kurzes Beispiel aus dem Code dieses Repositorys.

Code-Stil, Namen und Issues: [`CONVENTIONS.md`](CONVENTIONS.md). Regeln der CI: [`PIPELINE-CONVENTIONS.md`](PIPELINE-CONVENTIONS.md).
Prüfliste für neue Features: [Abschnitt 7](#7-prüfliste-für-neue-features).

---

## 1. Schichten und Abhängigkeiten

### 1.1 Abhängigkeiten zeigen nur nach unten

`core` → `servers` → `scene` → `editor`. Eine Schicht kennt nur die Schichten darunter. Server kennen keine Nodes,
Laufzeit-Code kennt keinen Editor. Vorbild: `RenderingServer` weiß nichts von `Sprite2D`; der pre-commit-Hook
`validate-includes` prüft einen Teil davon.

`servers/audio/audio_server.h` bindet nur `core` und `servers/audio` ein:

```cpp
#include "core/templates/safe_list.h"
#include "core/variant/variant.h"
#include "servers/audio/audio_bus_layout.h"
#include "servers/audio/audio_effect.h"
#include "servers/audio/audio_filter_sw.h"
```

### 1.2 Editor-Code nur mit `TOOLS_ENABLED`

Editor-Teile liegen in `editor/` oder `modules/<modul>/editor/` und werden nur im Editor-Build übersetzt. Export-Templates
enthalten sie nicht. Vorbild: jedes Modul mit Editor-Plugin (`modules/gltf`, `modules/gridmap`).

`modules/flow_graph/register_types.cpp`:

```cpp
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::add_by_type<FlowGraphEditorPlugin>();
		EditorPlugins::add_by_type<FlowGraphFormatsEditorPlugin>();
	}
#endif
```

### 1.3 Module sind optional

Wer ein Modul benutzt, prüft `MODULE_<NAME>_ENABLED`. Ein fehlendes Modul ergibt einen Fehlercode, keinen Absturz
und keinen Build-Fehler (das Minimal-Template baut ohne Module). Vorbild: `modules/modules_enabled.gen.h`.

`tests/core/io/test_resource_text_serializer_xml.cpp`:

```cpp
#include "modules/modules_enabled.gen.h"

#ifdef MODULE_YAML_ENABLED
#include "modules/yaml/yaml.h"
#endif
```

---

## 2. Welche Klassenart wofür

Jedes Feature wählt für jeden Teil die passende Art. Was abgespielt oder ausgeführt wird, folgt dem Muster
**Resource + Playback + Node** (Abschnitt 2.2–2.4), wie `AudioStream` + `AudioStreamPlayback` + `AudioStreamPlayer`.

| Art | Wofür | Vorbild in Godot | Bei uns |
|---|---|---|---|
| `Resource` | Daten und Assets: im Inspector bearbeiten, als Datei speichern, teilen | `Animation`, `AudioStream`, `TileSet` | `FlowGraph`, `Subtitle`, `SpriteSheet` |
| `RefCounted` (Playback) | Laufzeitzustand einer Nutzung | `AudioStreamPlayback`, `Tween` | `FlowGraphPlayback` |
| `Node` | dünne Verbindung zur Szene | `AudioStreamPlayer`, `AnimationPlayer` | `FlowGraphPlayer`, `SubtitlePlayer` |
| Server (Singleton) | gemeinsamer, szenenübergreifender Zustand | `AudioServer`, `TextServer` | `PathPrefixServer`, Settings-Server |
| Registry | austauschbare Implementierungen | `TextServerManager`, `ResourceLoader` | `ResourceTextSerializer::register_format` |
| Editor-Plugin | Bedienung im Editor | `EditorInspectorPlugin` | Katalog-, CSV-, Flow-Editor |

### 2.1 Resource: Daten, keine Laufzeit

Eine Resource beschreibt Daten. Sie wird im Inspector bearbeitet, gespeichert und zwischen Nutzern geteilt. Sie hält
keinen Zustand einer laufenden Nutzung (keine aktuelle Position, keinen Timer) und kennt ihr Dateiformat nicht.
Vorbild: `Animation` speichert Spuren, nicht die Abspielposition.

`modules/flow_graph/flow_graph.h`:

```cpp
class FlowGraph : public Resource {
	GDCLASS(FlowGraph, Resource);

public:
	static constexpr int NODE_ID_NONE = -1;
```

### 2.2 Playback: Zustand einer Nutzung als `RefCounted`

Der Zustand einer Nutzung (aktueller Knoten, Variablen, Zeit) liegt in einem eigenen `RefCounted`. So nutzen mehrere
Spieler dieselbe Resource gleichzeitig, und die Logik läuft auch ohne Szene (Tests, Server, Tools).
Vorbild: `AudioStreamPlayback`, `AnimationNodeStateMachinePlayback`.

`modules/flow_graph/flow_graph_playback.h`:

```cpp
class FlowGraphPlayback : public RefCounted {
	GDCLASS(FlowGraphPlayback, RefCounted);

public:
	static constexpr int START_NODE_DEFAULT = -1;
	static constexpr int HISTORY_MAX = 256;
```

### 2.3 Node: dünne Verbindung zur Szene

Ein Node gibt es nur, wenn das Feature in der Szene lebt: Lebenszeit im Baum, `_process`, Transform, Platzierung im
Editor. Er hält Resource und Playback und leitet weiter; die Logik steckt nicht im Node.
Vorbild: `AudioStreamPlayer` reicht Zeit und Lautstärke an seine Playback weiter.

`modules/flow_graph/flow_graph_player.cpp`:

```cpp
case NOTIFICATION_INTERNAL_PROCESS: {
	playback->process(get_process_delta_time());
} break;
case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
	playback->process(get_physics_process_delta_time());
} break;
```

### 2.4 Server: gemeinsamer Zustand als Singleton

Ein Server ist ein C++-Singleton (`Object`), registriert in `Engine`. Er besitzt Zustand, den viele Teile teilen
(Registrierungen, Geräte, Einstellungen), und ist der einzige Weg dorthin. Vorbild: `AudioServer`, `DisplayServer`.

`core/io/path_prefix_server.h`:

```cpp
class PathPrefixServer : public Object {
	GDCLASS(PathPrefixServer, Object);
	...
	static PathPrefixServer *get_singleton();
	Error register_prefix(const String &p_prefix, const Ref<PathPrefix> &p_path_prefix, const PackedStringArray &p_platforms = PackedStringArray());
```

### 2.5 Registry: Implementierungen anmelden statt fest einbauen

Wo es mehrere Implementierungen gibt (Formate, Treiber, Backends), meldet jede sich bei einer Registry an. Der Aufrufer
kennt nur die Schnittstelle. Vorbild: `TextServerManager::add_interface()`, `ResourceLoader::add_resource_format_loader()`.

`modules/toml/register_types.cpp`:

```cpp
ResourceSaver::add_resource_format_saver(resource_saver_text_data_toml);
ResourceTextSerializer::register_format(ResourceTextSerializer::FORMAT_TOML, &TOML::write_resource, &_parse_toml_format);
```

### 2.6 Editor-Plugin: nur Bedienung

Inspector-Plugins, Docks und Importer bedienen Resources und Server; sie enthalten keine Logik, die zur Laufzeit
gebraucht wird. Vorbild: `EditorInspectorPlugin`, `ResourceImporter`.

`editor/scene/texture/sprite_sheet_editor_plugin.cpp`:

```cpp
bool EditorInspectorPluginSpriteSheet::can_handle(Object *p_object) {
	return Object::cast_to<SpriteSheet>(p_object) != nullptr;
}
```

---

## 3. Server als API-Endpunkte

### 3.1 Viele Instanzen: Handles (`RID`) statt Objekte

Verwaltet ein Server viele gleichartige Einträge, die sich pro Frame ändern, hält er die Daten selbst und gibt `RID`s
heraus. Die API besteht aus Endpunkten: `<x>_create()`, `<x>_set_*()`, `<x>_get_*()`, `free_rid()`. Nodes und
Resources halten nur den RID. Server mit wenigen globalen Einstellungen (Settings-Server) bleiben Objekt-APIs.
Bestehende Server werden nicht umgebaut; neue Server mit vielen Instanzen werden so entworfen.
Vorbild: `RenderingServer`, `PhysicsServer2D`, `NavigationServer3D`.

`scene/main/canvas_item.cpp` (Godot):

```cpp
	canvas_item = RenderingServer::get_singleton()->canvas_item_create();
}

CanvasItem::~CanvasItem() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RenderingServer::get_singleton()->free_rid(canvas_item);
}
```

### 3.2 Der Server besitzt den Speicher

Die Einträge liegen in einem `RID_Owner` (oder `RID_PtrOwner`) des Servers. Er legt an, prüft Handles und gibt frei;
niemand außerhalb hält Zeiger darauf. Vorbild: alle Godot-Server.

`servers/rendering/renderer_canvas_cull.h` (Godot):

```cpp
RID_Owner<LightOccluderPolygon, true> canvas_light_occluder_polygon_owner;
RID_Owner<RendererCanvasRender::LightOccluderInstance, true> canvas_light_occluder_owner;
```

### 3.3 Signale und Aufrufe aus anderen Threads

Ein Server sendet Signale nur auf dem Main-Thread und nimmt Aufrufe aus anderen Threads über `call_deferred` oder eine
Command-Queue an. Im Web gibt es Builds ohne Threads: der Code darf keine eigenen Threads voraussetzen.
Vorbild: `RenderingServerWrapMT`, `MessageQueue`.

`core/io/path_prefix_server.cpp`:

```cpp
void PathPrefixServer::_emit_deferred(const StringName &p_signal, const String &p_prefix) {
	if (Thread::is_main_thread()) {
		emit_signal(p_signal, p_prefix);
	} else {
		call_deferred(SNAME("emit_signal"), p_signal, p_prefix);
	}
}
```

---

## 4. Data-Oriented unter der Haube

### 4.1 API objektorientiert, Speicher datenorientiert

Nach außen gibt es Objekte, Properties und Methoden. Innen liegen Daten, die zusammen verarbeitet werden, nebeneinander
in Arrays (Structure of Arrays), nicht verstreut in Objekten. Vorbild: der RenderingServer cullt Instanzen über
Arrays, nicht über Nodes.

`modules/flow_graph/flow_graph.h`:

```cpp
	PackedInt32Array node_ids;
	LocalVector<Ref<FlowNode>> nodes;
```

### 4.2 Engine-Container im heißen Pfad

Was pro Frame läuft, nutzt `LocalVector`, Packed-Arrays und `HashMap` mit `StringName`-Schlüsseln. Kein `Dictionary`,
kein `Variant`, kein `String`-Vergleich und keine Allokation pro Frame. `Variant` und `Dictionary` gehören an die
API-Grenze. Vorbild: `LocalVector` in den Renderern und Physik-Servern.

`modules/flow_graph/flow_graph_playback.h`:

```cpp
	LocalVector<Frame> stack;
	LocalVector<Evaluation> evaluations;
```

### 4.3 Änderungen sammeln, einmal verarbeiten

Viele Änderungen hintereinander lösen eine Neuberechnung aus, nicht viele. Ein Dirty-Flag merkt sich die Arbeit, ein
verzögerter Aufruf erledigt sie einmal. Vorbild: `_update_dirty_*`, `queue_redraw()`, `queue_sort()`.

`editor/data/csv_table_editor.cpp`:

```cpp
void CSVTableEditor::_csv_changed() {
	status_label->set_text(String());
	if (!update_queued) {
		update_queued = true;
		callable_mp(this, &CSVTableEditor::_update).call_deferred();
	}
}
```

### 4.4 Messen statt vermuten

Für heiße Pfade steht eine Messung im Issue (Zeit, Speicher, Plattformen inkl. Web), siehe Abschnitt „Measurements“ im
Wiki. Vorbild: Godots Benchmarks und Performance-Monitore.

Beispiel aus dem finalen Playtest von 0.2.1 (#389): die Tabelle eines Katalogs mit 1200 Keys öffnet in 229 ms, ein
neuer Key braucht 53 ms. Die Regel betrifft das Issue, nicht den Code.

---

## 5. Code-Ebene

### 5.1 Eine Anweisung pro Zeile

Jede Zeile tut eine Sache. Keine Zuweisung in Bedingungen, keine Ketten mit Seiteneffekten, Zwischenergebnisse bekommen
einen Namen. clang-format erzwingt das nicht. Vorbild: Godots Code-Stil.

`modules/flow_graph/flow_graph_player.cpp`:

```cpp
void FlowGraphPlayer::set_callback_mode_process(CallbackModeProcess p_mode) {
	ERR_FAIL_INDEX(p_mode, CALLBACK_MODE_PROCESS_MAX);
	callback_mode_process = p_mode;
	_update_processing();
}
```

### 5.2 Single Responsibility

Eine Klasse hat eine Aufgabe. Wird sie zu groß, wird sie geteilt: Daten (`Resource`), Zustand (`Playback`),
Szene (`Node`), Format (`ResourceFormatLoader`/`Saver`), Prüfung (eigene Validierungsklasse), Bedienung (Editor).
Vorbild: `TextServer` (Arbeit) und `TextServerManager` (Auswahl der Implementierung).

`modules/flow_graph/flow_graph.h`: Prüfergebnis als eigene Klasse neben dem Graphen.

```cpp
class FlowGraphValidation : public RefCounted {
	GDCLASS(FlowGraphValidation, RefCounted);
```

### 5.3 Separation of Concerns

Daten wissen nichts von ihrem Dateiformat, ihrer Darstellung oder ihrem Editor. Formate liegen in Loader/Saver oder
Format-Klassen, die Darstellung im Editor. Vorbild: `Image` kennt PNG nicht; `ImageLoaderPNG` und `ResourceSaverPNG` tun es.

`modules/toml/register_types.cpp`: das Format meldet sich an, die Resource bleibt unverändert.

```cpp
ResourceTextSerializer::register_format(ResourceTextSerializer::FORMAT_TOML, &TOML::write_resource, &_parse_toml_format);
```

### 5.4 Verträge am Anfang prüfen

Ungültige Eingaben werden am Anfang mit `ERR_FAIL_*` abgewiesen (früher Return, Meldung mit Grund). Keine Exceptions,
kein stilles Weitermachen. Vorbild: fast jede gebundene Methode in Godot.

`modules/flow_graph/flow_graph_player.cpp`:

```cpp
ERR_FAIL_INDEX(p_mode, CALLBACK_MODE_PROCESS_MAX);
```

### 5.5 Erweiterbar über Virtuals und Callables

Skripte und GDExtensions erweitern ein Feature über `GDVIRTUAL`-Methoden oder registrierte `Callable`s, nicht über
C++-Vererbung. Gebundene Methoden nehmen keine Rohzeiger. Vorbild: `_get_minimum_size`, `ResourceFormatLoader._load`.

`core/io/path_prefix_server.cpp`:

```cpp
bool PathPrefix::is_read_only() const {
	bool ret = false;
	GDVIRTUAL_CALL(_is_read_only, ret);
	return ret;
}
```

### 5.6 Lebenszyklus über Notifications, Änderungen über `changed`

Nodes reagieren in `_notification()` auf Lebenszyklus und Verarbeitung, nicht über eigene Timer oder Polling. Wer eine
Resource anzeigt, verbindet sich mit ihrem Signal `changed` und trennt sich wieder.
Vorbild: `NOTIFICATION_READY`, `Resource::connect_changed()`.

`editor/scene/texture/sprite_sheet_editor_plugin.cpp`:

```cpp
		atlas->disconnect_changed(callable_mp(this, &SpriteSheetRegionList::_atlas_changed));
	...
		atlas->connect_changed(callable_mp(this, &SpriteSheetRegionList::_atlas_changed));
```

---

## 6. Editor

### 6.1 Jede Änderung über Undo/Redo

Was der Nutzer im Editor ändert, läuft über `EditorUndoRedoManager` mit einem sprechenden Namen; ein Schritt pro
Handlung (beim Zusammenführen den Gegenstand in den Namen, damit nichts Fremdes verschmilzt).
Vorbild: alle Godot-Editoren.

`modules/flow_graph/editor/flow_graph_editor_plugin.cpp`:

```cpp
undo_redo->create_action(vformat(TTR("Set %s on Node %d"), p_property, p_id), UndoRedo::MERGE_ENDS, graph.ptr());
undo_redo->force_fixed_history();
undo_redo->add_do_property(node.ptr(), p_property, p_value);
undo_redo->add_undo_property(node.ptr(), p_property, node->get(p_property));
```

---

## 7. Prüfliste für neue Features

Gehört in „Feasibility to check“ des Issues, bevor die Umsetzung beginnt:

1. **Klassenarten** (Abschnitt 2): Welche Teile sind Resource, Playback, Node, Server, Registry, Editor? Warum ist ein Node nötig?
2. **Schichten** (Abschnitt 1): Abhängigkeiten nur nach unten; Editor-Teile hinter `TOOLS_ENABLED`; optionale Module geprüft.
3. **Server-API** (Abschnitt 3): Viele Instanzen → RID-Endpunkte; Threads und Web ohne Threads bedacht.
4. **Heißer Pfad** (Abschnitt 4): Was läuft pro Frame? Daten in Arrays, keine `Variant`/`Dictionary`/Allokation; Messung geplant.
5. **Erweiterung** (Abschnitt 5.5): Virtuals oder Callables für Skripte und GDExtension.
6. **Editor** (Abschnitt 6): Undo/Redo pro Handlung.
7. **CI** ([`PIPELINE-CONVENTIONS.md`](PIPELINE-CONVENTIONS.md)): Minimal-Template, SCU, Clang/GCC.
