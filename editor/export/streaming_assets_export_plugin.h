/**************************************************************************/
/*  streaming_assets_export_plugin.h                                      */
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

#include "editor/export/editor_export_plugin.h"

// Ships the streaming assets folder (`filesystem/streaming_assets/*`) with an export.
// The folder never goes through the import pipeline of the export: it is skipped
// and either copied unchanged next to the export (`raw` on desktop) or embedded
// as raw files into the main pack (the fallback on all other platforms).
// The project settings are registered by StreamingAssetsServer (core); this
// plugin only reads them and uses the same defaults when they are missing.
class StreamingAssetsExportPlugin : public EditorExportPlugin {
	GDCLASS(StreamingAssetsExportPlugin, EditorExportPlugin);

public:
	enum Mode {
		MODE_RAW,
		MODE_PACK,
	};

	enum Fallback {
		FALLBACK_EMBED,
		FALLBACK_NONE,
	};

	enum Placement {
		PLACEMENT_EXTERNAL,
		PLACEMENT_EMBED,
		PLACEMENT_NONE,
	};

	static constexpr const char *SETTING_ENABLED = "filesystem/streaming_assets/enabled";
	static constexpr const char *SETTING_SOURCE_FOLDER = "filesystem/streaming_assets/source_folder";
	static constexpr const char *SETTING_MODE = "filesystem/streaming_assets/mode";
	static constexpr const char *SETTING_TARGET_FOLDER = "filesystem/streaming_assets/target_folder";
	static constexpr const char *SETTING_FALLBACK = "filesystem/streaming_assets/fallback";

	static constexpr const char *DEFAULT_SOURCE_FOLDER = "res://streaming_assets";
	static constexpr const char *DEFAULT_TARGET_FOLDER = "{exe_name}_Data/StreamingAssets";

private:
	bool active = false;
	Placement placement = PLACEMENT_NONE;
	String source_folder; // `res://` path, simplified, without a trailing slash.
	String target_folder; // Expanded, relative to the export folder.
	String export_path; // Absolute path of the exported executable or pack.
	PackedStringArray files; // Relative to `source_folder`.

	static void _collect_files(const String &p_root, const String &p_relative, int p_depth, PackedStringArray &r_files);
	static Mode _parse_mode(const Variant &p_value);
	static Fallback _parse_fallback(const Variant &p_value);
	Variant _get_setting(const char *p_name, const Variant &p_default) const;
	void _add_message(EditorExportPlatform::ExportMessageType p_type, const String &p_message) const;
	void _reset();
	void _copy_external();

protected:
	static void _bind_methods();

	virtual String get_name() const override { return "StreamingAssets"; }

	virtual void _export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) override;
	virtual void _export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) override;
	virtual bool _begin_customize_resources(const Ref<EditorExportPlatform> &p_platform, const Vector<String> &p_features) override;
	virtual void _export_end() override;

public:
	static String expand_template(const String &p_template, const String &p_exe_name);
	static bool is_valid_target_folder(const String &p_folder);
	static bool is_path_in_folder(const String &p_path, const String &p_folder);
	static bool is_file_excluded(const String &p_name);
	static PackedStringArray collect_files(const String &p_folder);
	static Placement resolve_placement(const PackedStringArray &p_features, const String &p_export_path, Fallback p_fallback);
};

VARIANT_ENUM_CAST(StreamingAssetsExportPlugin::Mode);
VARIANT_ENUM_CAST(StreamingAssetsExportPlugin::Fallback);
VARIANT_ENUM_CAST(StreamingAssetsExportPlugin::Placement);
