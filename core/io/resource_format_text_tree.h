/**************************************************************************/
/*  resource_format_text_tree.h                                           */
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

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/io/resource_uid.h"

// Loaders and savers for the property-based text formats of ResourceTextSerializer.
// A format implements _get_extension(), _parse_text() and _write_text(); modules can add more formats the same way.

class ResourceFormatLoaderTextTree : public ResourceFormatLoader {
	GDSOFTCLASS(ResourceFormatLoaderTextTree, ResourceFormatLoader);

public:
	// Bytes read to find the type marker of a file.
	static constexpr int MARKER_HEAD_SIZE = 4096;

protected:
	virtual String _get_extension() const = 0;
	// Returns ERR_FILE_UNRECOGNIZED (without printing) if the file is not a resource file of this format and another loader may take it.
	virtual Error _parse_text(const String &p_text, const String &p_type_hint, Ref<Resource> &r_resource, String &r_error) const = 0;
	// Type stored in the file, or an empty string. `p_text` is only the head of the file (MARKER_HEAD_SIZE
	// bytes): the type marker must be written at the start of the file.
	virtual String _get_marker_type(const String &p_text) const { return String(); }
	// False for formats without a type marker (CSV): the type then comes only from the type hint of load().
	virtual bool _has_type_marker() const { return true; }

	// The first MARKER_HEAD_SIZE bytes of a file as text (a code point cut at the end is dropped).
	static String _read_head(const String &p_path);
	// The value of the first `$type` key in `p_head` (JSON, INI, TOML, YAML), if it names a Resource class.
	static String _scan_key_marker(const String &p_head);
	String _get_file_type(const String &p_path, String *r_script_class) const;

public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual bool recognize_path(const String &p_path, const String &p_for_type = String()) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;
	virtual String get_resource_script_class(const String &p_path) const override;

	// Text files that players edit: no `*.uid` files next to them.
	virtual ResourceUID::ID get_resource_uid(const String &p_path) const override { return ResourceUID::INVALID_ID; }
	virtual bool has_custom_uid_support() const override { return true; }
};

class ResourceFormatSaverTextTree : public ResourceFormatSaver {
	GDSOFTCLASS(ResourceFormatSaverTextTree, ResourceFormatSaver);

protected:
	virtual String _get_extension() const = 0;
	virtual Error _write_text(const Ref<Resource> &p_resource, String &r_text, String &r_error) const = 0;

public:
	virtual Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0) override;
	virtual void get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const override;
	virtual bool recognize(const Ref<Resource> &p_resource) const override;
	virtual bool recognize_path(const Ref<Resource> &p_resource, const String &p_path) const override;
};

// JSON: one object per resource. Registered in front of ResourceFormatLoaderJSON; files without "$type" go to that loader.
class ResourceFormatLoaderResourceJSON : public ResourceFormatLoaderTextTree {
	GDSOFTCLASS(ResourceFormatLoaderResourceJSON, ResourceFormatLoaderTextTree);

protected:
	virtual String _get_extension() const override { return "json"; }
	virtual Error _parse_text(const String &p_text, const String &p_type_hint, Ref<Resource> &r_resource, String &r_error) const override;
	virtual String _get_marker_type(const String &p_text) const override;

public:
	virtual bool handles_type(const String &p_type) const override;
};

class ResourceFormatSaverResourceJSON : public ResourceFormatSaverTextTree {
	GDSOFTCLASS(ResourceFormatSaverResourceJSON, ResourceFormatSaverTextTree);

protected:
	virtual String _get_extension() const override { return "json"; }
	virtual Error _write_text(const Ref<Resource> &p_resource, String &r_text, String &r_error) const override;
};

// INI: one section per resource ([root], [root/entries/0]).
class ResourceFormatLoaderResourceINI : public ResourceFormatLoaderTextTree {
	GDSOFTCLASS(ResourceFormatLoaderResourceINI, ResourceFormatLoaderTextTree);

protected:
	virtual String _get_extension() const override { return "ini"; }
	virtual Error _parse_text(const String &p_text, const String &p_type_hint, Ref<Resource> &r_resource, String &r_error) const override;
	virtual String _get_marker_type(const String &p_text) const override;
};

class ResourceFormatSaverResourceINI : public ResourceFormatSaverTextTree {
	GDSOFTCLASS(ResourceFormatSaverResourceINI, ResourceFormatSaverTextTree);

protected:
	virtual String _get_extension() const override { return "ini"; }
	virtual Error _write_text(const Ref<Resource> &p_resource, String &r_text, String &r_error) const override;
};

// CSV: list resources only (one typed array of flat resources); the type comes from the type hint of load().
class ResourceFormatLoaderResourceCSV : public ResourceFormatLoaderTextTree {
	GDSOFTCLASS(ResourceFormatLoaderResourceCSV, ResourceFormatLoaderTextTree);

protected:
	virtual String _get_extension() const override { return "csv"; }
	virtual Error _parse_text(const String &p_text, const String &p_type_hint, Ref<Resource> &r_resource, String &r_error) const override;
	// CSV has no type marker: type checks and plain files need no file read.
	virtual bool _has_type_marker() const override { return false; }
};

class ResourceFormatSaverResourceCSV : public ResourceFormatSaverTextTree {
	GDSOFTCLASS(ResourceFormatSaverResourceCSV, ResourceFormatSaverTextTree);

protected:
	virtual String _get_extension() const override { return "csv"; }
	virtual Error _write_text(const Ref<Resource> &p_resource, String &r_text, String &r_error) const override;
};

// XML: root element <resource type="..."> with one child element per property. Files without that root are left to other loaders.
class ResourceFormatLoaderResourceXML : public ResourceFormatLoaderTextTree {
	GDSOFTCLASS(ResourceFormatLoaderResourceXML, ResourceFormatLoaderTextTree);

protected:
	virtual String _get_extension() const override { return "xml"; }
	virtual Error _parse_text(const String &p_text, const String &p_type_hint, Ref<Resource> &r_resource, String &r_error) const override;
	virtual String _get_marker_type(const String &p_text) const override;
};

class ResourceFormatSaverResourceXML : public ResourceFormatSaverTextTree {
	GDSOFTCLASS(ResourceFormatSaverResourceXML, ResourceFormatSaverTextTree);

protected:
	virtual String _get_extension() const override { return "xml"; }
	virtual Error _write_text(const Ref<Resource> &p_resource, String &r_text, String &r_error) const override;
};
