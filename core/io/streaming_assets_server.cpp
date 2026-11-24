/**************************************************************************/
/*  streaming_assets_server.cpp                                           */
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

#include "streaming_assets_server.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/file_access_pack.h"
#include "core/io/path_prefix_server.h"
#include "core/io/pck_package_server.h"
#include "core/object/class_db.h"
#include "core/os/os.h"

namespace {

// The logic of `streaming://`. It asks the server for the current root on every call,
// so that the locked prefix never has to be registered again.
class PathPrefixStreamingAssets : public PathPrefix {
	GDSOFTCLASS(PathPrefixStreamingAssets, PathPrefix);

public:
	virtual String globalize_path(const String &p_prefix, const String &p_path) const override {
		const StreamingAssetsServer *server = StreamingAssetsServer::get_singleton();
		if (!server) {
			return String();
		}
		// An OS folder or a `res://` folder: PathPrefixServer follows `res://` itself, which
		// also lets FileAccess and DirAccess read the embedded copy from the main pack (#87).
		const String streaming_root = server->get_root();
		if (streaming_root.is_empty()) {
			return String();
		}
		const String rest = p_path.substr(p_prefix.length());
		return rest.is_empty() ? streaming_root : streaming_root.path_join(rest);
	}

	virtual String localize_path(const String &p_prefix, const String &p_global_path) const override {
		const StreamingAssetsServer *server = StreamingAssetsServer::get_singleton();
		if (!server) {
			return String();
		}
		String streaming_root = server->get_root();
		if (streaming_root.is_empty()) {
			return String();
		}
		if (streaming_root.begins_with("res://") && ProjectSettings::get_singleton()) {
			streaming_root = ProjectSettings::get_singleton()->globalize_path(streaming_root);
		}
		streaming_root = streaming_root.replace_char('\\', '/');
		const String global_path = p_global_path.replace_char('\\', '/');
		if (global_path == streaming_root) {
			return p_prefix;
		}
		if (global_path.begins_with(streaming_root + "/")) {
			return p_prefix + global_path.substr(streaming_root.length() + 1);
		}
		return String();
	}

	virtual bool is_available() const override {
		const StreamingAssetsServer *server = StreamingAssetsServer::get_singleton();
		return server && server->get_location() != StreamingAssetsServer::LOCATION_NONE;
	}

	virtual bool is_read_only() const override {
		// Shipped streaming assets are never written by the game. Only the source folder in the editor is writable.
		const StreamingAssetsServer *server = StreamingAssetsServer::get_singleton();
		return !server || server->get_location() != StreamingAssetsServer::LOCATION_SOURCE_FOLDER;
	}
};

// Removes `.` and `..` from a relative path. Returns false if the path leaves its root or is absolute.
bool _normalize_relative_path(const String &p_path, String &r_path) {
	const String path = p_path.replace_char('\\', '/');
	if (path.is_absolute_path() || path.contains_char(':')) {
		return false;
	}
	Vector<String> parts;
	const Vector<String> input = path.split("/", false);
	for (const String &part : input) {
		if (part == ".") {
			continue;
		}
		if (part == "..") {
			if (parts.is_empty()) {
				return false;
			}
			parts.remove_at(parts.size() - 1);
			continue;
		}
		parts.push_back(part);
	}
	r_path = String("/").join(parts);
	return true;
}

// A relative folder below the executable or bundle folder: no `.`, `..`, drive letters or unexpanded placeholders.
bool _is_valid_target_folder(const String &p_folder) {
	if (p_folder.is_empty() || p_folder.is_absolute_path() || p_folder.contains_char(':')) {
		return false;
	}
	const Vector<String> parts = p_folder.split("/");
	for (const String &part : parts) {
		if (part.is_empty() || part == "." || part == ".." || part.contains_char('{') || part.contains_char('}')) {
			return false;
		}
	}
	return true;
}

Variant _get_setting(const char *p_name, const Variant &p_default) {
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (!project_settings || !project_settings->has_setting(p_name)) {
		return p_default;
	}
	const Variant value = GLOBAL_GET(p_name);
	return value.get_type() == Variant::NIL ? p_default : value;
}

} // namespace

StreamingAssetsServer *StreamingAssetsServer::singleton = nullptr;

StreamingAssetsServer *StreamingAssetsServer::get_singleton() {
	return singleton;
}

// Static helpers.

void StreamingAssetsServer::register_settings() {
	GLOBAL_DEF_RST_BASIC(SETTING_ENABLED, false);
	GLOBAL_DEF_RST_BASIC(PropertyInfo(Variant::STRING, SETTING_SOURCE_FOLDER, PROPERTY_HINT_DIR), DEFAULT_SOURCE_FOLDER);
	GLOBAL_DEF_RST_BASIC(PropertyInfo(Variant::INT, SETTING_MODE, PROPERTY_HINT_ENUM, "Raw,Pack"), int(MODE_RAW));
	GLOBAL_DEF_RST(SETTING_TARGET_FOLDER, DEFAULT_TARGET_FOLDER);
	GLOBAL_DEF_RST(SETTING_PACK_FILE, DEFAULT_PACK_FILE);
	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, SETTING_FALLBACK, PROPERTY_HINT_ENUM, "Embed,None"), int(FALLBACK_EMBED));
}

String StreamingAssetsServer::expand_template(const String &p_template, const String &p_exe_name) {
	String result = p_template.replace("{exe_name}", p_exe_name);
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings && result.contains("{app_name}")) {
		String app_name = String(_get_setting("application/config/name", String())).strip_edges().validate_filename();
		if (app_name.is_empty()) {
			app_name = p_exe_name;
		}
		result = result.replace("{app_name}", app_name);
	}
	if (project_settings && result.contains("{version}")) {
		result = result.replace("{version}", String(_get_setting("application/config/version", String())).strip_edges().validate_filename());
	}
	return result;
}

String StreamingAssetsServer::get_exe_name() {
	const String executable = OS::get_singleton()->get_executable_path().replace_char('\\', '/');
	const String dir = executable.get_base_dir();
	if (dir.ends_with(".app/Contents/MacOS")) {
		// macOS: the export names the bundle (`MyGame.app`), the binary inside may be named differently.
		return dir.get_base_dir().get_base_dir().get_file().get_basename();
	}
	return executable.get_file().get_basename();
}

String StreamingAssetsServer::_strip_trailing_slash(const String &p_path) {
	String path = p_path;
	while (path.length() > 1 && path.ends_with("/") && !path.ends_with("://") && !path.ends_with(":/")) {
		path = path.substr(0, path.length() - 1);
	}
	return path;
}

String StreamingAssetsServer::_sanitize_source_folder(const String &p_folder) {
	const String folder = _strip_trailing_slash(p_folder.strip_edges().replace_char('\\', '/').simplify_path());
	if (!folder.begins_with("res://") || folder.ends_with("://")) {
		return String();
	}
	return folder;
}

// Startup.

void StreamingAssetsServer::setup() {
	Configuration config;
	config.enabled = bool(_get_setting(SETTING_ENABLED, false));
	config.mode = int(_get_setting(SETTING_MODE, int(MODE_RAW))) == MODE_PACK ? MODE_PACK : MODE_RAW;
	config.fallback = int(_get_setting(SETTING_FALLBACK, int(FALLBACK_EMBED))) == FALLBACK_NONE ? FALLBACK_NONE : FALLBACK_EMBED;
	config.source_folder = String(_get_setting(SETTING_SOURCE_FOLDER, DEFAULT_SOURCE_FOLDER));

	const String exe_name = get_exe_name();
	config.target_folder = expand_template(String(_get_setting(SETTING_TARGET_FOLDER, DEFAULT_TARGET_FOLDER)).strip_edges(), exe_name);
	config.pack_file = expand_template(String(_get_setting(SETTING_PACK_FILE, DEFAULT_PACK_FILE)).strip_edges(), exe_name);

	String main_pack;
	const PackedData *packed_data = PackedData::get_singleton();
	if (packed_data) {
		for (const PackedData::PackInfo &info : packed_data->get_packs()) {
			if (info.main_pack) {
				main_pack = info.path;
				break;
			}
		}
	}
	if (!main_pack.is_empty() && !main_pack.begins_with("res://")) {
		main_pack = main_pack.replace_char('\\', '/');
		if (main_pack.is_relative_path()) {
			main_pack = OS::get_singleton()->get_cwd().replace_char('\\', '/').path_join(main_pack);
		}
		config.main_pack_dir = main_pack.simplify_path().get_base_dir();
	}

#ifdef TOOLS_ENABLED
	// The editor and games run from it read the source folder. A tools build that runs an exported pack does not.
	config.from_source = main_pack.is_empty();
#endif

#if defined(MACOS_ENABLED) || defined(APPLE_EMBEDDED_ENABLED)
	config.bundle = true;
	config.bundle_resource_dir = OS::get_singleton()->get_bundle_resource_dir().replace_char('\\', '/');
#endif
#if !defined(ANDROID_ENABLED) && !defined(WEB_ENABLED)
	config.external = true;
#endif
	config.executable_dir = OS::get_singleton()->get_executable_path().replace_char('\\', '/').get_base_dir();
	if (config.main_pack_dir.is_empty() && config.external) {
		config.main_pack_dir = config.executable_dir;
	}

#ifdef DEBUG_ENABLED
	// Lets debug builds and the editor test the external folder layout. Only the `=` form is
	// accepted, so that the folder is never taken as the positional scene argument.
	const List<String> args = OS::get_singleton()->get_cmdline_args();
	for (const String &arg : args) {
		if (arg.begins_with(COMMAND_LINE_OVERRIDE)) {
			String folder = arg.substr(String(COMMAND_LINE_OVERRIDE).length()).strip_edges().replace_char('\\', '/');
			if (!folder.is_empty() && folder.is_relative_path()) {
				folder = OS::get_singleton()->get_cwd().replace_char('\\', '/').path_join(folder);
			}
			config.override_folder = folder.simplify_path();
		}
	}
#endif

	configure(config);
}

void StreamingAssetsServer::configure(const Configuration &p_config) {
	// Errors are collected and printed at the end, never while the lock is held.
	Vector<String> errors;
	Location new_location = LOCATION_NONE;
	String new_root;
	String new_mounted_pack;

	String new_source_folder = _sanitize_source_folder(p_config.source_folder);
	if (new_source_folder.is_empty()) {
		if (p_config.enabled) {
			errors.push_back(vformat("The streaming assets source folder \"%s\" must be a folder below \"res://\". \"%s\" is used.", p_config.source_folder, DEFAULT_SOURCE_FOLDER));
		}
		new_source_folder = DEFAULT_SOURCE_FOLDER;
	}

	const String target_folder = _strip_trailing_slash(p_config.target_folder.strip_edges().replace_char('\\', '/'));

	if (!p_config.enabled) {
		new_location = LOCATION_NONE;
	} else if (!p_config.override_folder.is_empty()) {
		if (DirAccess::dir_exists_absolute(p_config.override_folder)) {
			new_location = LOCATION_EXTERNAL_FOLDER;
			new_root = _strip_trailing_slash(p_config.override_folder);
		} else {
			errors.push_back(vformat("The streaming assets folder \"%s\" given on the command line does not exist.", p_config.override_folder));
		}
	} else if (p_config.from_source) {
		new_location = LOCATION_SOURCE_FOLDER;
		new_root = new_source_folder;
	} else if (p_config.mode == MODE_PACK) {
		// The hook for the streaming pack (#91): a second pack next to the main pack.
		const String pack_file = p_config.pack_file.strip_edges();
		if (pack_file.is_empty() || !pack_file.is_valid_filename()) {
			errors.push_back(vformat("The streaming assets pack file \"%s\" must be a file name without folders.", pack_file));
		} else if (!p_config.main_pack_dir.is_empty()) {
			const String pack_path = p_config.main_pack_dir.path_join(pack_file);
			if (FileAccess::exists(pack_path)) {
				Error err = OK;
				PckPackageServer *pck_server = PckPackageServer::get_singleton();
				if (!pck_server) {
					err = ERR_UNAVAILABLE;
				} else if (mounted_pack != pack_path && !pck_server->is_mounted(pack_path)) {
					err = pck_server->mount(pack_path, true);
				}
				if (err == OK) {
					new_location = LOCATION_PACK;
					new_root = new_source_folder;
					new_mounted_pack = pack_path;
				} else {
					errors.push_back(vformat("Could not mount the streaming assets pack \"%s\" (error %d).", pack_path, int(err)));
				}
			}
		}
		if (new_location == LOCATION_NONE && p_config.fallback == FALLBACK_NONE && errors.is_empty()) {
			errors.push_back(vformat("The streaming assets pack \"%s\" was not found next to the main pack.", pack_file));
		}
	} else {
		if (!_is_valid_target_folder(target_folder)) {
			errors.push_back(vformat("The streaming assets target folder \"%s\" must be a relative folder without \".\", \"..\" or unknown placeholders.", target_folder));
		} else if (p_config.bundle && !p_config.bundle_resource_dir.is_empty()) {
			const String folder = p_config.bundle_resource_dir.path_join(target_folder);
			if (DirAccess::dir_exists_absolute(folder)) {
				new_location = LOCATION_BUNDLE;
				new_root = folder;
			}
		}
		if (new_location == LOCATION_NONE && p_config.external && !p_config.executable_dir.is_empty() && _is_valid_target_folder(target_folder)) {
			const String folder = p_config.executable_dir.path_join(target_folder);
			if (DirAccess::dir_exists_absolute(folder)) {
				new_location = LOCATION_EXTERNAL_FOLDER;
				new_root = folder;
			}
		}
	}

	if (p_config.enabled && new_location == LOCATION_NONE && p_config.fallback == FALLBACK_EMBED && !p_config.from_source) {
		// The export embedded the folder into the main pack: `streaming://` aliases to it.
		new_location = LOCATION_EMBEDDED;
		new_root = new_source_folder;
	}

	{
		RWLockWrite write_lock(lock);
		enabled = p_config.enabled;
		mode = p_config.mode;
		fallback = p_config.fallback;
		source_folder = new_source_folder;
		location = new_location;
		root = new_root;
		if (!new_mounted_pack.is_empty()) {
			mounted_pack = new_mounted_pack;
		}
	}

	for (const String &error : errors) {
		ERR_PRINT(error);
	}
}

void StreamingAssetsServer::_register_prefix() {
	PathPrefixServer *prefix_server = PathPrefixServer::get_singleton();
	ERR_FAIL_NULL_MSG(prefix_server, "StreamingAssetsServer must be created after PathPrefixServer.");
	Ref<PathPrefixStreamingAssets> logic;
	logic.instantiate();
	// Locked like the engine prefixes: games and extensions can't replace or remove it.
	prefix_server->_register(PREFIX, logic, PackedStringArray(), true);
}

// State.

bool StreamingAssetsServer::is_enabled() const {
	RWLockRead read_lock(lock);
	return enabled;
}

StreamingAssetsServer::Mode StreamingAssetsServer::get_mode() const {
	RWLockRead read_lock(lock);
	return mode;
}

StreamingAssetsServer::Fallback StreamingAssetsServer::get_fallback() const {
	RWLockRead read_lock(lock);
	return fallback;
}

StreamingAssetsServer::Location StreamingAssetsServer::get_location() const {
	RWLockRead read_lock(lock);
	return location;
}

String StreamingAssetsServer::get_source_folder() const {
	RWLockRead read_lock(lock);
	return source_folder;
}

String StreamingAssetsServer::get_root() const {
	RWLockRead read_lock(lock);
	return root;
}

String StreamingAssetsServer::get_os_path(const String &p_streaming_path) const {
	Location current_location;
	String current_root;
	{
		RWLockRead read_lock(lock);
		current_location = location;
		current_root = root;
	}
	if (current_location != LOCATION_SOURCE_FOLDER && current_location != LOCATION_EXTERNAL_FOLDER && current_location != LOCATION_BUNDLE) {
		return String();
	}

	String relative;
	if (p_streaming_path.begins_with(PREFIX)) {
		relative = p_streaming_path.substr(String(PREFIX).length());
	} else if (p_streaming_path.contains("://")) {
		return String();
	} else {
		relative = p_streaming_path;
	}
	String normalized;
	if (!_normalize_relative_path(relative, normalized)) {
		return String();
	}

	if (current_root.begins_with("res://")) {
		ProjectSettings *project_settings = ProjectSettings::get_singleton();
		if (!project_settings) {
			return String();
		}
		current_root = project_settings->globalize_path(current_root);
		if (current_root.begins_with("res://")) {
			return String(); // `res://` is not on disk.
		}
	}
	current_root = current_root.replace_char('\\', '/');
	return normalized.is_empty() ? current_root : current_root.path_join(normalized);
}

void StreamingAssetsServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_enabled"), &StreamingAssetsServer::is_enabled);
	ClassDB::bind_method(D_METHOD("get_mode"), &StreamingAssetsServer::get_mode);
	ClassDB::bind_method(D_METHOD("get_fallback"), &StreamingAssetsServer::get_fallback);
	ClassDB::bind_method(D_METHOD("get_location"), &StreamingAssetsServer::get_location);
	ClassDB::bind_method(D_METHOD("get_source_folder"), &StreamingAssetsServer::get_source_folder);
	ClassDB::bind_method(D_METHOD("get_root"), &StreamingAssetsServer::get_root);
	ClassDB::bind_method(D_METHOD("get_os_path", "streaming_path"), &StreamingAssetsServer::get_os_path);
	ClassDB::bind_static_method("StreamingAssetsServer", D_METHOD("expand_template", "template", "exe_name"), &StreamingAssetsServer::expand_template);
	ClassDB::bind_static_method("StreamingAssetsServer", D_METHOD("get_exe_name"), &StreamingAssetsServer::get_exe_name);

	BIND_ENUM_CONSTANT(MODE_RAW);
	BIND_ENUM_CONSTANT(MODE_PACK);

	BIND_ENUM_CONSTANT(FALLBACK_EMBED);
	BIND_ENUM_CONSTANT(FALLBACK_NONE);

	BIND_ENUM_CONSTANT(LOCATION_SOURCE_FOLDER);
	BIND_ENUM_CONSTANT(LOCATION_EXTERNAL_FOLDER);
	BIND_ENUM_CONSTANT(LOCATION_BUNDLE);
	BIND_ENUM_CONSTANT(LOCATION_EMBEDDED);
	BIND_ENUM_CONSTANT(LOCATION_PACK);
	BIND_ENUM_CONSTANT(LOCATION_NONE);
}

StreamingAssetsServer::StreamingAssetsServer() {
	singleton = this;
	_register_prefix();
}

StreamingAssetsServer::~StreamingAssetsServer() {
	singleton = nullptr;
}
