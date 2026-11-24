/**************************************************************************/
/*  list_settings_server.h                                                */
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

#include "servers/settings/settings_server.h"

// A settings server whose settings resource holds a list of entry resources (audio buses,
// windows, TTS personas, ...). The list is one typed Array property of the settings resource.
// Entries are found by their index or by the value of a key property (for example `bus_name`).
// A list domain may be stored as CSV when its settings resource has no other changed values.
class ListSettingsServer : public SettingsServer {
	GDCLASS(ListSettingsServer, SettingsServer);

	bool format_warned_csv = false;

	Array _get_list() const;
	void _set_list(const Array &p_list);
	Variant _validate_entry(int p_index, const Ref<Resource> &p_entry, const StringName &p_name, const Variant &p_value) const;

protected:
	static void _bind_methods();

	virtual ResourceTextSerializer::Format _get_file_format() override;
	virtual void _validate_loaded_settings(const Ref<Resource> &p_settings) override;

	GDVIRTUAL0RC(StringName, _get_list_property)
	GDVIRTUAL0RC(StringName, _get_entry_key)
	GDVIRTUAL3RC(Variant, _validate_entry_value, int, StringName, Variant)

public:
	// The Array property with the entries; by default the first Array property of the settings resource.
	virtual StringName get_list_property() const;
	// The property of an entry that identifies it in find_entry(); empty by default.
	virtual StringName get_entry_key() const;

	int get_entry_count() const;
	Ref<Resource> get_entry(int p_index) const;
	TypedArray<Resource> get_entries() const;
	int find_entry(const Variant &p_key) const;
	void add_entry(const Ref<Resource> &p_entry, int p_at_index = -1);
	void remove_entry(int p_index);
	void move_entry(int p_from, int p_to);
	void clear_entries();
	void set_entry_value(int p_index, const StringName &p_name, const Variant &p_value);
	Variant get_entry_value(int p_index, const StringName &p_name) const;
};
