# Glossary

Terms of **truely-godot-engine** (this fork of [godotengine/godot](https://github.com/godotengine/godot)) and the Godot terms they build on, with main source files, issues and [wiki](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki) pages. A **file index** by feature area follows the terms.

- Rules: [`AGENTS.md`](AGENTS.md), [`truely/CONVENTIONS.md`](truely/CONVENTIONS.md), [`truely/FEATURE-WORKFLOW.md`](truely/FEATURE-WORKFLOW.md), [`truely/README.md`](truely/README.md). Order and dependencies of the work: tracker issue #32.
- A new public term, class or setting gets an entry here (alphabetical, 1 to 3 sentences, paths, issues, wiki page).
- Paths use `name.*` for the `.h` and `.cpp` pair.

## Terms

### A

**Abnahme checks** (acceptance checks): the five fixed points that end every feature and refactoring issue: C++ API, bindings, docs (class reference and wiki), test build, GDScript and C# test in the test project. Ticked in the issue; results go in as a comment. See `truely/CONVENTIONS.md` (section 3) and `truely/FEATURE-WORKFLOW.md` (section 6).

**AccessibilitySettings** / **AccessibilitySettingsServer**: settings domain for accessibility options; `AccessibilityColorScheme` is its color resource. In code: `servers/settings/accessibility_*.*`. Wiki: [`Accessibility-Settings`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Accessibility-Settings), [`Class-AccessibilitySettingsServer`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Class-AccessibilitySettingsServer).

**AudioBusSettings** / **AudioBusSettingsServer**: settings domain that stores and applies volume and mute of the audio buses. In code: `servers/settings/audio_bus_settings_server.*`, `servers/settings/audio_settings.*`. Wiki: [`Audio-Bus-Settings`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Audio-Bus-Settings).

**AudioCommand** / **AudioCommandList**: data description of an audio action that can be stored, edited in the inspector and run later. In code: `scene/audio/audio_command.*`. Wiki: [`Audio-Commands`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Audio-Commands).

**AudioDeviceSettings** / **AudioDeviceSettingsServer**: settings domain for the output and input audio device. In code: `servers/settings/audio_device_settings_server.*`. Wiki: [`Audio-Device-Settings`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Audio-Device-Settings).

**AudioMixerServer**: C++ singleton that manages named audio channels (`AudioMixerChannel`) and a layout (`AudioMixerLayout`) on top of the audio buses. It lives in `scene/audio/` (not `servers/`) because `AudioStream` lives in `scene/`. In code: `scene/audio/audio_mixer_server.*`, `scene/resources/audio/audio_mixer_*.*`. Issues: #13. Wiki: [`Audio-Mixer`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Audio-Mixer), [`Design-Decisions`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Design-Decisions). Not to confuse with `AudioServer` (Godot's bus server) or `AudioBusSettingsServer` (saved player settings).

### B

**Batch CI** (CI in batches): the full CI of all platforms runs only on pushes to `truely-godot-engine-releases` (about once per 8 to 10 merged features), not on feature branches or `truely-godot-engine`. Feature branches are checked locally (Windows, Mono, unit tests, GDScript and C# checks); manual run: `gh workflow run runner.yml --ref <branch>`. In code: `.github/workflows/runner.yml`. Issues: #46, #273. See `truely/FEATURE-WORKFLOW.md` (2.4).

**Bindings**: registration of a C++ class for scripts in `_bind_methods()` (`ClassDB::bind_method`, `ADD_PROPERTY`, `ADD_SIGNAL`, `BIND_ENUM_CONSTANT`). In this fork the C++ object is the API: GDScript sees it directly, C# through the generated glue. See `truely/CONVENTIONS.md` (1.1).

### C

**Cached resource paths**: `ResourceLoader` lists the paths of all resources currently in the resource cache, filtered by type, extension or folder. In code: `core/io/resource_loader.*`. Wiki: [`Cached-Resource-Paths`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Cached-Resource-Paths).

**Camera3DSettings** / **Camera3DSettingsServer**: settings domain for 3D camera options. In code: `servers/settings/camera_3d_settings*.*`. Wiki: [`Camera-3D-Settings`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Camera-3D-Settings).

**Children of a type**: find all descendants of a `Node` of a given type; C# gets typed `GetChildren<T>()` and `FindChildren<T>()`. In code: `scene/main/node.*`, `modules/mono/`. Wiki: [`Children-of-a-Type`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Children-of-a-Type).

**Class reference**: the XML description of a class in `doc/classes/<Class>.xml` (written as `--doctool` writes it; module classes in `modules/<module>/doc_classes/`) and one wiki page `Class-<Class>` per class. Mandatory for every new or changed class.

**Concept issue**: an issue that ends with a proposal and a "Decision" comment instead of code; decisions are collected on the wiki page `Design-Decisions` (for example #4, #6, #13, #14, #31).

**CSV**: tabular text format in core with the same `parse()` / `stringify()` / `serialize()` shape as the other text formats. In code: `core/io/csv_format.*`. Wiki: [`CSV`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/CSV).

### D

**Decision (comment)**: open questions of an issue are decided by the orchestrator and posted as a comment headed "Decision". See `.agents/project.md`.

**Design decisions**: wiki page with the concept decisions and their follow-up work. Wiki: [`Design-Decisions`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Design-Decisions).

**Recursive directory listing**: `DirAccess` lists all files or folders below a path in one C++ call, optionally filtered by extension. In code: `core/io/dir_access.*`. Wiki: [`Recursive-Directory-Listing`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Recursive-Directory-Listing).

### E

**EngineSettings** / **EngineSettingsServer**: settings domain for engine-wide options. In code: `servers/settings/engine_settings*.*`. Wiki: [`Engine-Settings`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Engine-Settings).

**`exec://`**: path prefix for the folder of the running executable (in the editor and in games started from it: the project folder). See **Path prefix**. Not to confuse with `res://` (project resources, read-only in exports) and `user://` (writable user data).

### F

**Feature branch**: branch `<nn>-feature|refactoring|bug|concept-<name>` from `truely-godot-engine`, with the issue number (at least two digits) as prefix, for example `03-feature-path-prefix-server`. Squash-merged into `truely-godot-engine` and **never deleted**. See `truely/README.md` and `truely/FEATURE-WORKFLOW.md` (2.1).

**File manifest** (`FilesManifest`): data description of a set of files (folders, extensions, single files, a type) that `ResourceLoader` lists or loads in one call, in the editor, in exports and in packs. `FilesManifestServer` holds named manifests, `FilesManifestValidation` reports problems. In code: `core/io/files_manifest*.*`. Issues: #8. Wiki: [`File-Manifests`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/File-Manifests).

### G

**Glue** (C# glue): C# wrapper code generated from the built engine with `--generate-mono-glue modules/mono/glue`. It must be regenerated after every change of bound classes, otherwise new classes are missing in C#. Singletons get a static `Foo` and an instance wrapper `FooInstance`. In code: `modules/mono/`. See `truely/FEATURE-WORKFLOW.md` (section 3).

**Graphics preset** (`GraphicsPreset`, `GraphicsPresetSettings`, `GraphicsPresetSettingsServer`): a named bundle of render quality, shadow, texture, effect and window values applied in one call. Formerly `VideoPreset` (#243). In code: `servers/settings/graphics_preset*.*`. Wiki: [`Graphics-Presets`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Graphics-Presets). Not to confuse with `VideoStreamPlayer` (video playback).

### H

**HTTPRequestTarget** / **HTTPResponse**: `HTTPRequestTarget` is a `Resource` with URL, method, headers and body of a request, editable in the inspector; `HTTPRequest` takes it and returns an `HTTPResponse`. In code: `scene/resources/http_request_target.*`, `scene/main/http_response.*`, `editor/inspector/http_request_target_editor_plugin.*`. Wiki: [`HTTP-Requests`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/HTTP-Requests).

### I

**INI**: INI text format (Variant syntax plus a plain mode). In code: `core/io/ini_format.*`. Wiki: [`INI`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/INI).

**InputAction** / **InputActions** / **InputActionRebinder**: resources and helper for built-in actions and player key rebinding. In code: `core/input/input_action*.*`, `core/input/builtin_input_actions.*`, `servers/settings/input_action_rebinder.*`. Wiki: [`Input-Settings`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Input-Settings).

**InputIconServer** / **InputIconSet**: pick the button prompt icon (key, mouse, gamepad family) for the player's current device. In code: `scene/main/input_icon_server.*`, `scene/resources/input_icon_set.*`. Wiki: [`Input-Icons`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Input-Icons).

**InputMapSettingsServer**: settings domain that saves and applies rebound `InputMap` events (records in `input_map_settings_records.h`). In code: `servers/settings/input_map_settings_*.*`. Wiki: [`Input-Map-Settings`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Input-Map-Settings).

**InputSettings** / **InputSettingsServer**: settings domain for input options. In code: `servers/settings/input_settings*.*`. Wiki: [`Input-Settings`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Input-Settings).

### L

**LanguageSettings** / **LanguageSettingsServer**: settings domain for the game's locale. In code: `servers/settings/language_settings*.*`. Wiki: [`Language-Settings`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Language-Settings).

**ListSettingsServer**: base for settings domains that hold a list of records (for example the TTS personas). In code: `servers/settings/list_settings_server.*`.

**Load result** (`ResourceLoadResult`, `ResourceSaveResult`): return value with error code and message for loading and saving; C# `TryLoad<T>`. In code: `core/io/resource_result.*`. Wiki: [`Load-Results`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Load-Results).

**Loader index**: lookup of `ResourceFormatLoader` by file extension through a copy-on-write snapshot; the old limit of 64 loaders and savers is gone. In code: `core/io/resource_loader.*`. Issues: #2, #6. Wiki: [`Loader-Index`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Loader-Index), [`Resource-Format-Limits`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Resource-Format-Limits).

### M

**Mono build**: engine build with `module_mono_enabled=yes` (`./truely/build.ps1 module_mono_enabled=yes`), followed by glue generation and `build_assemblies.py`, so C# checks can run. Required for every feature check.

### O

**Opus audio** (`AudioStreamOpus`, `ResourceImporterOpus`): Opus audio import and runtime loading, carried from the Godot PR godotengine/godot#117921. In code: `modules/opus/`. Wiki: [`Opus-Audio`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Opus-Audio).

### P

**Path prefix** (`PathPrefix`, `PathPrefixServer`): own prefixes such as `mods://` or `appdata://` usable in every file path like `res://` and `user://`; a prefix maps to a folder or custom logic and may differ per platform. Built-ins known to the server: `res://`, `user://`, `uid://`, `pipe://`, `exec://`. `PathInfo` describes a resolved path. In code: `core/io/path_prefix_server.*`, `core/io/path_info.*`. Issues: #3. Wiki: [`Path-Prefixes`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Path-Prefixes).

**PCK package** (`PCKPackage`, `PCKPackageServer`, `PCKPackageEntry`, `PCKBuildPlan`, `PCKExtractResult`, `PCKPackageExporter`): open a `.pck` without mounting it, list, read and verify single files, build and extract packs, mount and unmount at run time (mods, DLC, patches). In code: `core/io/pck_*.*`, `core/io/resource_format_pck.*`, `editor/export/pck_*.*`, `editor/file_system/pck_extract_dialog.*`. Wiki: [`PCK-Packages`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/PCK-Packages).

**PerformanceRecorder**: native singleton that writes one row per frame to a ring buffer and a CSV file, summarizes the run (mean, percentiles) and checks a `PerformanceBudget` (for CI). Profile resource: `PerformanceRecordingProfile`. In code: `main/performance_*.*`, hook in `main/main.cpp`. Issues: #31. Wiki: [`Performance-Recorder`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Performance-Recorder).

**PR branch**: branch from `master` for a contribution to Godot; it must start at `master` so the PR does not contain own features. See `truely/README.md`.

**PreloadList**: resource that names what `ResourceDatabase` loads in one call. In code: `core/io/preload_list.*`. Wiki: [`Resource-Database`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Resource-Database).

**Process memory**: `OS` methods that return the memory of the whole process as the platform tools show it (works in release exports, unlike `get_static_memory_usage()`). In code: `core/os/os.*`. Wiki: [`Process-Memory`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Process-Memory).

### R

**Release** / **release names**: builds published from `truely-godot-engine-releases` after a green CI, named `Truely_Godot<Version>-<Status>_<Hash>_<Platform>` (for example `Truely_Godot4.8-beta5_a1b2c3d_win64.zip`, `..._mono_win64.zip`, `..._export_templates.tpz`); tag `truely-godot4.8-beta5-a1b2c3d`. A new naming scheme is tracked in #265. In code: `.github/workflows/truely_release.yml`, `.github/truely/`, `truely/release.ps1`, `truely/pack-release.ps1`. See `truely/README.md`.

**RenderEffectSettings**, **RenderQualitySettings**, **ShadowSettings**, **TextureSettings**, **TextureStreamingSettings**, **WindowSettings**, **XRSettings**, **TTSPersonaSettings**: further settings domains, each a `Resource` (the values, editable in the inspector) plus a `...Server` (load, apply, save). In code: `servers/settings/<name>*.*`. Wiki: [`Render-Effect-Settings`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Render-Effect-Settings), [`Render-Quality-Settings`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Render-Quality-Settings), [`Shadow-Settings`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Shadow-Settings), [`Texture-Settings`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Texture-Settings), [`Texture-Streaming-Settings`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Texture-Streaming-Settings), [`Window-Settings`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Window-Settings), [`XR-Settings`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/XR-Settings), [`TTS-Persona-Settings`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/TTS-Persona-Settings). The former `Video*Settings` names were renamed (#243).

**`res://`**: Godot's project resource path (read-only in exports). See also `user://`, `exec://` and **Path prefix**.

**Resource database** (`ResourceDatabase`): singleton that keeps strong references to loaded resources and finds them by key, type or pattern. In code: `core/io/resource_database.*`. Wiki: [`Resource-Database`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Resource-Database).

**ResourceTextSerializer** / **SerializerOptions**: one entry point that serializes a resource to any of the six text formats (JSON, INI, CSV, XML, TOML, YAML) with typed options. In code: `core/io/resource_text_serializer.*`, `core/io/serializer_options.*`. Wiki: [`Text-Serialization`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Text-Serialization).

### S

**SCU** (single compilation unit builds): `scu_builders.py` combines the `.cpp` files of a folder into one translation unit to compile faster; new source files must stay SCU-safe (no clashing file-local names). In code: `scu_builders.py`, `SCsub` files.

**Screenshots** (`ScreenshotServer`): captures the main window, a `Window`, a `SubViewport` or a screen to a project-wide folder with a file name pattern; nothing is overwritten. In code: `scene/main/screenshot_server.*`. Wiki: [`Screenshots`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Screenshots).

**SeedServer**: singleton for named, reproducible random streams. In code: `core/math/seed_server.*`. Issues: #11. Wiki: [`Seeds`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Seeds).

**Server** (singleton): a C++ class derived from `Object` with `get_singleton()`, registered with `Engine::add_singleton`, no node, fully bound for scripts. Most features of this fork are servers (`TTSServer`, `SubtitleServer`, `ScreenshotServer`, `PathPrefixServer`, ...). See `truely/CONVENTIONS.md` (1.3).

**Settings domain**: one group of player settings (audio buses, language, input map, window, ...) handled by one `SettingsServer`.

**SettingsServer** / **SettingsServerManager**: `SettingsServer` is the virtual base (extensible from GDScript, C# and GDExtension) of one domain; `SettingsServerManager` is the registry that loads, applies and saves all of them. Default storage: `user://settings`. In code: `servers/settings/settings_server.*`, `servers/settings/settings_server_manager.*`. Issues: #14. Wiki: [`Settings-Servers`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Settings-Servers).

**SpriteSheet**: resource that cuts a texture into frames, with an editor plugin. In code: `scene/resources/sprite_sheet.*`, `editor/scene/texture/sprite_sheet_editor_plugin.*`. Wiki: [`Sprite-Sheet`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Sprite-Sheet).

**SQLite** (`SQLiteDatabase`, `SQLiteStatement`, `SQLiteColumn`): module with an SQLite database for GDScript and C#; the VFS works on `user://`. `.db`/`.sqlite` files load as `SQLiteDatabase` resources; a schema API (tables, columns, row counts) and a read-only table overview in the Inspector (`modules/sqlite/editor/`). In code: `modules/sqlite/`. Wiki: [`SQLite`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/SQLite).

**Streaming assets** (`StreamingAssetsServer`, `StreamingAssetsPackBuilder`, `StreamingAssetsExportPlugin`): files in `res://streaming_assets` that the export ships unchanged next to the main pack (videos with seeking, data opened by OS path, moddable files). In code: `core/io/streaming_assets_server.*`, `editor/export/streaming_assets_*.*`. Wiki: [`Streaming-Assets`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Streaming-Assets).

**Subtitle** (`Subtitle`, `SubtitleSegment`, `SubtitleServer`, `SubtitleValidation`): timed text with SRT and other formats, handled by a server. In code: `servers/subtitle/`. Wiki: [`Subtitles`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Subtitles).

### T

**Test project**: separate public repository `godot-custom-build-project` with one folder per feature and checks in GDScript and C#; `new-feature.ps1 -Name <feature-branch>` creates the folder. See `truely/FEATURE-WORKFLOW.md` (section 4).

**Text serialization formats**: JSON, INI, CSV, XML, TOML, YAML. Data level: `parse()`, `parse_string()`, `stringify()`; resource level: `serialize()`, `deserialize()`. In code: `core/io/*_format.*`, `modules/toml/`, `modules/yaml/`. Wiki: [`Text-Serialization`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Text-Serialization), [`JSON`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/JSON), [`INI`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/INI), [`CSV`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/CSV), [`XML`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/XML), [`TOML`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/TOML), [`YAML`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/YAML).

**TileSet coordinate system**: `TileSet` options `swap_x_and_y_axis`, `flip_x` and `flip_y` change the cell numbering of a tile map without moving tiles (replaced `CoordinateAxes`, #261). In code: `scene/resources/2d/tile_set.*`, `modules/tilemap/`. Wiki: [`TileSet-Coordinate-Axes`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/TileSet-Coordinate-Axes).

**Tracker** (issue #32, label `flag: tracker`): lists the issues, their order and dependencies; `gh issue list --label "flag: tracker"`.

**`truely-godot-engine`**: default branch, the own engine (receives `master`, PR branches and squash-merged feature branches). **`truely-godot-engine-releases`**: release branch, fast-forward only via `truely/release.ps1`; a push starts the full CI. **`master`**: pure copy of `upstream/master`, no own commits. See `truely/README.md`.

**TTSServer** / **TTSPersona**: text to speech with voice personas; `TTSPersonaSettingsServer` stores the player's choice. In code: `servers/tts/`, `servers/settings/tts_persona_settings*.*`. Wiki: [`Text-to-Speech`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Text-to-Speech), [`TTS-Persona-Settings`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/TTS-Persona-Settings).

### U

**Upstream PRs** (carried Godot PRs): Godot pull requests kept as branches (for example `195-feature-opus-audio`) and merged into `truely-godot-engine` by `truely/merge-chain.ps1` in the order of `truely/upstream-prs.tsv`.

**`user://`**: Godot's writable user data path (IndexedDB in the Web export). Settings servers and screenshots default to it.

### V

**VideoStreamWebm**: WebM (VP8/VP9, alpha, seeking) video stream loadable at run time, carried from godotengine/godot#119299, experimental. In code: `modules/webm/`. Wiki: [`WebM-Video`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/WebM-Video), [`Video-Files`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/Video-Files).

### W

**Wiki**: public documentation of all features: a guide per feature and `Class-<Class>` per class, with a numbered table of contents (`python truely/wiki-toc.py <wiki-folder>`) and examples in GDScript, C# and C++ (GDExtension). Mandatory for every issue.

### X

**XML** / **XMLWriter**: XML documents and plain data in core; `XMLWriter` complements Godot's `XMLParser`. In code: `core/io/xml_format.*`, `core/io/xml_writer.*`. Wiki: [`XML`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/XML).

### Y

**YAML** / **TOML**: text formats as modules. In code: `modules/yaml/`, `modules/toml/`. Wiki: [`YAML`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/YAML), [`TOML`](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki/TOML).

## File index

Feature area to main files. Each class has `doc/classes/<Class>.xml` (module classes: `modules/<module>/doc_classes/`). Bindings are in each class's `_bind_methods()`; registration is in `core/register_core_types.cpp`, `scene/register_scene_types.cpp`, `servers/register_server_types.cpp`, `main/main.cpp` and `modules/<module>/register_types.cpp`.

| Area | Classes | Source | Tests |
|---|---|---|---|
| Settings framework | `SettingsServer`, `SettingsServerManager`, `ListSettingsServer` | `servers/settings/settings_server*.*`, `settings_server_manager.*`, `list_settings_server.*` | `tests/servers/test_settings_server.cpp`, `test_settings_float_precision.cpp` |
| Audio, graphics, window settings | `AudioBusSettings*`, `AudioDeviceSettings*`, `RenderQualitySettings*`, `RenderEffectSettings*`, `ShadowSettings*`, `TextureSettings*`, `TextureStreamingSettings*`, `GraphicsPreset*`, `WindowSettings*`, `Camera3DSettings*`, `XRSettings*` | `servers/settings/<name>*.*` | `tests/servers/test_<name>_settings_server.cpp` |
| Language, input, accessibility, engine, TTS settings | `LanguageSettings*`, `InputSettings*`, `InputMapSettings*`, `InputActionRebinder`, `AccessibilitySettings*`, `EngineSettings*`, `TTSPersonaSettings*` | `servers/settings/`, `core/input/` | `tests/servers/test_*_settings_server.cpp`, `test_input_action_rebinder.cpp` |
| Path prefixes | `PathPrefixServer`, `PathPrefix`, `PathInfo` | `core/io/path_prefix_server.*`, `core/io/path_info.*` | `tests/core/io/test_path_prefix_server.cpp` |
| Resource loading, formats, results | `ResourceLoader`, `ResourceSaver`, `ResourceFormatLoader`, `ResourceLoadResult`, `ResourceSaveResult` | `core/io/resource_loader.*`, `core/io/resource_result.*`, `core/io/resource_format_callable.*` | `tests/core/io/test_resource_formats.cpp`, `test_resource_result.cpp`, `test_resource_format_callable.cpp` |
| File manifests, resource database | `FilesManifest`, `FilesManifestServer`, `ResourceDatabase`, `PreloadList` | `core/io/files_manifest*.*`, `core/io/resource_database.*`, `core/io/preload_list.*` | `tests/core/io/test_files_manifest*.cpp`, `test_resource_database.cpp` |
| Text serialization | `ResourceTextSerializer`, `SerializerOptions`, `JSON`, `INI`, `CSV`, `XML`, `XMLWriter`, `TOML`, `YAML` | `core/io/resource_text_serializer.*`, `core/io/*_format.*`, `core/io/xml_writer.*`, `modules/toml/`, `modules/yaml/` | `tests/core/io/test_resource_text_serializer*.cpp`, `test_xml_writer.cpp`, `modules/toml/tests/`, `modules/yaml/tests/` |
| Packs and streaming assets | `PCKPackage*`, `PCKBuildPlan`, `StreamingAssets*` | `core/io/pck_*.*`, `core/io/resource_format_pck.*`, `core/io/streaming_assets_server.*`, `editor/export/`, `editor/inspector/pck_package_editor_plugin.*` | `tests/core/io/test_pck_package_server.cpp`, `test_resource_format_pck.cpp`, `test_streaming_assets_server.cpp` |
| Runtime file formats | image, audio, video, scene and font loaders and savers | `scene/resources/*_resource_format.*`, `modules/{mp3,vorbis,theora,opus,webm,gltf,fbx}/` | `modules/*/tests/` |
| Databases | `SQLiteDatabase`, `SQLiteStatement`, `SQLiteColumn` | `modules/sqlite/` | `modules/sqlite/tests/` |
| Audio mixer, commands | `AudioMixerServer`, `AudioMixerChannel`, `AudioMixerLayout`, `AudioCommand`, `AudioCommandList` | `scene/audio/`, `scene/resources/audio/` | `tests/scene/test_audio_*.cpp` |
| Subtitles, TTS | `SubtitleServer`, `Subtitle`, `TTSServer`, `TTSPersona` | `servers/subtitle/`, `servers/tts/` | `tests/servers/test_subtitle*.cpp`, `test_tts_server.cpp` |
| Input icons, screenshots, HTTP | `InputIconServer`, `InputIconSet`, `ScreenshotServer`, `HTTPRequestTarget`, `HTTPResponse` | `scene/main/`, `scene/resources/` | `tests/scene/` |
| 2D | `SpriteSheet`, `TileSet`, `TileMapLayer` | `scene/resources/sprite_sheet.*`, `scene/resources/2d/tile_set.*`, `modules/tilemap/` | `modules/tilemap/tests/` |
| Random | `SeedServer` | `core/math/seed_server.*` | `tests/core/math/test_seed_server.cpp` |
| Performance | `PerformanceRecorder`, `PerformanceBudget`, `PerformanceRecordingProfile`, `PerformanceSummary` | `main/performance_*.*` | `tests/main/` |
| C# integration | glue, `GetChildren<T>`, `TryLoad<T>`, .NET metrics | `modules/mono/`, `modules/mono/dotnet_metric_source.*` | `modules/mono/tests/` |
| Tooling and CI | build, sync, release | `truely/*.ps1`, `.github/workflows/`, `.github/truely/`, `.github/ISSUE_TEMPLATE/` | - |
| Rules and docs | conventions, workflow, project state | `AGENTS.md`, `truely/*.md`, `.agents/project.md`, `doc/classes/` | - |
