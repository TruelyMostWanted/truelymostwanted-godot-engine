/**************************************************************************/
/*  resource_format_yaml.h                                                */
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

#include "core/io/resource_format_text_tree.h"

// YAML: one mapping per resource; nested resources are nested mappings. One loader and saver per extension
// (`yaml` and `yml`).
class ResourceFormatLoaderResourceYAML : public ResourceFormatLoaderTextTree {
	GDSOFTCLASS(ResourceFormatLoaderResourceYAML, ResourceFormatLoaderTextTree);

	String extension = "yaml";

protected:
	virtual String _get_extension() const override { return extension; }
	virtual Error _parse_text(const String &p_text, const String &p_type_hint, Ref<Resource> &r_resource, String &r_error) const override;
	virtual String _get_marker_type(const String &p_text) const override;

public:
	void set_extension(const String &p_extension) { extension = p_extension; }
};

class ResourceFormatSaverResourceYAML : public ResourceFormatSaverTextTree {
	GDSOFTCLASS(ResourceFormatSaverResourceYAML, ResourceFormatSaverTextTree);

	String extension = "yaml";

protected:
	virtual String _get_extension() const override { return extension; }
	virtual Error _write_text(const Ref<Resource> &p_resource, String &r_text, String &r_error) const override;

public:
	void set_extension(const String &p_extension) { extension = p_extension; }
};
