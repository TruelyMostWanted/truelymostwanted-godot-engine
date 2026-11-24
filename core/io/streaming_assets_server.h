/**************************************************************************/
/*  streaming_assets_server.h                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/object/object.h"
#include "core/os/rw_lock.h"

// Owns the streaming assets settings (`filesystem/streaming_assets/*`) and the `streaming://` prefix.
// `setup()` runs once at startup, after the project settings and the main pack are loaded. It finds
// where the streaming assets are on this run (the source folder in the editor, a folder next to the
// executable, the app bundle resources, a mounted pack or the copy embedded in the main pack) and
// points `streaming://` there. In `pack` mode it mounts the streaming pack.
// The state is written at startup and read from any thread afterwards.
class StreamingAssetsServer : public Object {
	GDCLASS(StreamingAssetsServer, Object);

public:
	enum Mode {
		MODE_RAW,
		MODE_PACK,
	};

	enum Fallback {
		FALLBACK_EMBED,
		FALLBACK_NONE,
	};

	enum Location {
		LOCATION_SOURCE_FOLDER,
		LOCATION_EXTERNAL_FOLDER,
		LOCATION_BUNDLE,
		LOCATION_EMBEDDED,
		LOCATION_PACK,
		LOCATION_NONE,
	};

	static constexpr const char *PREFIX = "streaming://";
	static constexpr const char *COMMAND_LINE_OVERRIDE = "--streaming-assets-path=";

	static constexpr const char *SETTING_ENABLED = "filesystem/streaming_assets/enabled";
	static constexpr const char *SETTING_SOURCE_FOLDER = "filesystem/streaming_assets/source_folder";
	static constexpr const char *SETTING_MODE = "filesystem/streaming_assets/mode";
	static constexpr const char *SETTING_TARGET_FOLDER = "filesystem/streaming_assets/target_folder";
	static constexpr const char *SETTING_PACK_FILE = "filesystem/streaming_assets/pack_file";
	static constexpr const char *SETTING_FALLBACK = "filesystem/streaming_assets/fallback";

	static constexpr const char *DEFAULT_SOURCE_FOLDER = "res://streaming_assets";
	static constexpr const char *DEFAULT_TARGET_FOLDER = "{exe_name}_Data/StreamingAssets";
	static constexpr const char *DEFAULT_PACK_FILE = "{exe_name}.streaming.pck";

	// What `configure()` works with. `setup()` fills it from the project settings and the running process.
	struct Configuration {
		bool enabled = false;
		Mode mode = MODE_RAW;
		Fallback fallback = FALLBACK_EMBED;
		String source_folder = DEFAULT_SOURCE_FOLDER; // `res://` folder.
		String target_folder; // Expanded, relative.
		String pack_file; // Expanded file name.
		bool from_source = false; // Editor, or a game run from the editor.
		String override_folder; // `--streaming-assets-path=`: an OS folder that wins over everything else.
		bool bundle = false; // macOS, iOS, visionOS: look into the bundle resources.
		bool external = false; // A folder next to the executable can exist (not Android or Web).
		String executable_dir; // OS folder of the executable.
		String bundle_resource_dir; // OS folder of the bundle resources.
		String main_pack_dir; // Folder of the main pack, empty if there is none.
	};

private:
	static StreamingAssetsServer *singleton;

	mutable RWLock lock;
	bool enabled = false;
	Mode mode = MODE_RAW;
	Fallback fallback = FALLBACK_EMBED;
	Location location = LOCATION_NONE;
	String source_folder = DEFAULT_SOURCE_FOLDER;
	String root; // OS folder, or `res://` folder. Empty for LOCATION_NONE.
	String mounted_pack; // The pack mounted by `configure()`, if any.

	static String _strip_trailing_slash(const String &p_path);
	static String _sanitize_source_folder(const String &p_folder);
	void _register_prefix();

protected:
	static void _bind_methods();

public:
	static StreamingAssetsServer *get_singleton();

	static void register_settings();
	static String expand_template(const String &p_template, const String &p_exe_name);
	static String get_exe_name();

	// Called once at startup (`register_core_extensions()`): reads the settings and calls `configure()`.
	void setup();
	// Resolves the location for `p_config`, mounts the pack in `pack` mode and updates `streaming://`.
	// Used by `setup()` and the unit tests. Not bound.
	void configure(const Configuration &p_config);

	bool is_enabled() const;
	Mode get_mode() const;
	Fallback get_fallback() const;
	Location get_location() const;
	String get_source_folder() const;
	String get_root() const;
	String get_os_path(const String &p_streaming_path) const;

	StreamingAssetsServer();
	~StreamingAssetsServer();
};

VARIANT_ENUM_CAST(StreamingAssetsServer::Mode);
VARIANT_ENUM_CAST(StreamingAssetsServer::Fallback);
VARIANT_ENUM_CAST(StreamingAssetsServer::Location);
