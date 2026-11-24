/**************************************************************************/
/*  resource_format_fbx.cpp                                               */
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

#include "resource_format_fbx.h"

#include "fbx_document.h"
#include "fbx_state.h"

void ResourceFormatLoaderFBX::_get_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("fbx");
}

Ref<GLTFDocument> ResourceFormatLoaderFBX::_create_document() const {
	Ref<FBXDocument> document;
	document.instantiate();
	return document;
}

Ref<GLTFState> ResourceFormatLoaderFBX::_create_state() const {
	Ref<FBXState> state;
	state.instantiate();
	return state;
}

uint32_t ResourceFormatLoaderFBX::_get_import_flags() const {
	// Same as the editor's ufbx importer.
	return GLTFDocument::IMPORT_FLAG_USE_NAMED_SKIN_BINDS;
}
