/**************************************************************************/
/*  sqlite_vfs.cpp                                                        */
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

#include "sqlite_vfs.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"

#include <sqlite3.h>

#include <cstring>

namespace {

// Data behind one open SQLite file. Either a FileAccess or a memory buffer.
struct GodotSQLiteFileData {
	Ref<FileAccess> file;
	String path;
	Vector<uint8_t> memory;
	bool in_memory = false;
	bool read_only = false;
};

// SQLite allocates szOsFile bytes and passes them to xOpen. The first member
// must be sqlite3_file; the C++ data lives behind a pointer.
struct GodotSQLiteFile {
	sqlite3_file base;
	GodotSQLiteFileData *data;
};

GodotSQLiteFileData *_get_data(sqlite3_file *p_file) {
	return reinterpret_cast<GodotSQLiteFile *>(p_file)->data;
}

int _close(sqlite3_file *p_file) {
	GodotSQLiteFile *file = reinterpret_cast<GodotSQLiteFile *>(p_file);
	if (file->data) {
		if (file->data->file.is_valid()) {
			file->data->file->close();
		}
		memdelete(file->data);
		file->data = nullptr;
	}
	return SQLITE_OK;
}

int _read(sqlite3_file *p_file, void *r_buffer, int p_amount, sqlite3_int64 p_offset) {
	GodotSQLiteFileData *data = _get_data(p_file);
	uint8_t *dst = static_cast<uint8_t *>(r_buffer);
	if (p_amount <= 0) {
		return SQLITE_OK;
	}
	if (p_offset < 0) {
		return SQLITE_IOERR_READ;
	}

	uint64_t read = 0;
	if (data->in_memory) {
		const int64_t size = data->memory.size();
		if (p_offset < size) {
			read = MIN((int64_t)p_amount, size - p_offset);
			memcpy(dst, data->memory.ptr() + p_offset, read);
		}
	} else {
		const uint64_t length = data->file->get_length();
		if ((uint64_t)p_offset < length) {
			data->file->seek(p_offset);
			read = data->file->get_buffer(dst, MIN((uint64_t)p_amount, length - p_offset));
		}
	}

	if (read < (uint64_t)p_amount) {
		// SQLite requires the rest of the buffer to be zeroed on a short read.
		memset(dst + read, 0, p_amount - read);
		return SQLITE_IOERR_SHORT_READ;
	}
	return SQLITE_OK;
}

int _write(sqlite3_file *p_file, const void *p_buffer, int p_amount, sqlite3_int64 p_offset) {
	GodotSQLiteFileData *data = _get_data(p_file);
	if (data->read_only || p_offset < 0) {
		return SQLITE_READONLY;
	}
	if (p_amount <= 0) {
		return SQLITE_OK;
	}

	if (data->in_memory) {
		const int64_t end = p_offset + p_amount;
		if (end > data->memory.size()) {
			const int64_t old_size = data->memory.size();
			if (data->memory.resize(end) != OK) {
				return SQLITE_IOERR_NOMEM;
			}
			if (p_offset > old_size) {
				memset(data->memory.ptrw() + old_size, 0, p_offset - old_size);
			}
		}
		memcpy(data->memory.ptrw() + p_offset, p_buffer, p_amount);
		return SQLITE_OK;
	}

	const uint64_t length = data->file->get_length();
	if ((uint64_t)p_offset > length) {
		// Fill the gap explicitly, not every FileAccess can seek past the end.
		data->file->seek(length);
		uint8_t zeros[4096] = {};
		uint64_t gap = p_offset - length;
		while (gap > 0) {
			const uint64_t chunk = MIN(gap, (uint64_t)sizeof(zeros));
			if (!data->file->store_buffer(zeros, chunk)) {
				return SQLITE_IOERR_WRITE;
			}
			gap -= chunk;
		}
	} else {
		data->file->seek(p_offset);
	}
	if (!data->file->store_buffer(static_cast<const uint8_t *>(p_buffer), p_amount)) {
		return SQLITE_IOERR_WRITE;
	}
	return SQLITE_OK;
}

int _truncate(sqlite3_file *p_file, sqlite3_int64 p_size) {
	GodotSQLiteFileData *data = _get_data(p_file);
	if (data->read_only) {
		return SQLITE_READONLY;
	}
	if (data->in_memory) {
		if (p_size < data->memory.size()) {
			data->memory.resize(p_size);
		}
		return SQLITE_OK;
	}
	if ((uint64_t)p_size >= data->file->get_length()) {
		return SQLITE_OK;
	}
	return data->file->resize(p_size) == OK ? SQLITE_OK : SQLITE_IOERR_TRUNCATE;
}

int _sync(sqlite3_file *p_file, int p_flags) {
	GodotSQLiteFileData *data = _get_data(p_file);
	if (data->in_memory || data->read_only) {
		return SQLITE_OK;
	}
	data->file->flush();
#ifdef WEB_ENABLED
	// user:// on the Web is synced to IndexedDB when a written file is closed.
	// Closing and reopening the file persists every commit.
	data->file->close();
	data->file = FileAccess::open(data->path, FileAccess::READ_WRITE);
	if (data->file.is_null()) {
		return SQLITE_IOERR_FSYNC;
	}
#endif
	return SQLITE_OK;
}

int _file_size(sqlite3_file *p_file, sqlite3_int64 *r_size) {
	GodotSQLiteFileData *data = _get_data(p_file);
	*r_size = data->in_memory ? data->memory.size() : (sqlite3_int64)data->file->get_length();
	return SQLITE_OK;
}

// Locks are no-ops: one connection per database file and process.
int _lock(sqlite3_file *p_file, int p_level) {
	return SQLITE_OK;
}

int _unlock(sqlite3_file *p_file, int p_level) {
	return SQLITE_OK;
}

int _check_reserved_lock(sqlite3_file *p_file, int *r_result) {
	*r_result = 0;
	return SQLITE_OK;
}

int _file_control(sqlite3_file *p_file, int p_op, void *p_arg) {
	return SQLITE_NOTFOUND;
}

int _sector_size(sqlite3_file *p_file) {
	return 4096;
}

int _device_characteristics(sqlite3_file *p_file) {
	GodotSQLiteFileData *data = _get_data(p_file);
	// Databases in res:// never change while the game runs.
	return data->read_only && SQLiteVFS::is_read_only_path(data->path.utf8().get_data()) ? SQLITE_IOCAP_IMMUTABLE : 0;
}

const sqlite3_io_methods io_methods = {
	1, // iVersion: no shared memory (WAL is compiled out), no memory mapping.
	_close,
	_read,
	_write,
	_truncate,
	_sync,
	_file_size,
	_lock,
	_unlock,
	_check_reserved_lock,
	_file_control,
	_sector_size,
	_device_characteristics,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
};

int _open(sqlite3_vfs *p_vfs, sqlite3_filename p_name, sqlite3_file *p_file, int p_flags, int *r_out_flags) {
	GodotSQLiteFile *file = reinterpret_cast<GodotSQLiteFile *>(p_file);
	file->base.pMethods = nullptr; // xClose is not called when xOpen fails.
	file->data = nullptr;

	GodotSQLiteFileData *data = memnew(GodotSQLiteFileData);
	int out_flags = p_flags;

	if (p_name == nullptr || (p_flags & SQLITE_OPEN_DELETEONCLOSE)) {
		// Temporary files live in memory: not every platform has a writable temp folder.
		data->in_memory = true;
	} else {
		data->path = String::utf8(p_name);
		const bool res_path = SQLiteVFS::is_read_only_path(p_name);
		const bool exists = FileAccess::exists(data->path);

		if ((p_flags & SQLITE_OPEN_READWRITE) && !res_path) {
			if (exists) {
				data->file = FileAccess::open(data->path, FileAccess::READ_WRITE);
			} else if (p_flags & SQLITE_OPEN_CREATE) {
				data->file = FileAccess::open(data->path, FileAccess::WRITE_READ);
			}
		}
		if (data->file.is_null() && exists && !(p_flags & SQLITE_OPEN_EXCLUSIVE)) {
			// Read-only request, res://, or no write permission: fall back to reading.
			data->file = FileAccess::open(data->path, FileAccess::READ);
			data->read_only = true;
			out_flags = (out_flags & ~(SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)) | SQLITE_OPEN_READONLY;
		}
		if (data->file.is_null()) {
			memdelete(data);
			return SQLITE_CANTOPEN;
		}
	}

	if (r_out_flags) {
		*r_out_flags = out_flags;
	}
	file->data = data;
	file->base.pMethods = &io_methods;
	return SQLITE_OK;
}

int _delete(sqlite3_vfs *p_vfs, const char *p_name, int p_sync_dir) {
	const String path = String::utf8(p_name);
	if (!FileAccess::exists(path)) {
		return SQLITE_IOERR_DELETE_NOENT;
	}
	if (SQLiteVFS::is_read_only_path(p_name)) {
		return SQLITE_IOERR_DELETE;
	}
	return DirAccess::remove_absolute(path) == OK ? SQLITE_OK : SQLITE_IOERR_DELETE;
}

int _access(sqlite3_vfs *p_vfs, const char *p_name, int p_flags, int *r_result) {
	const bool exists = FileAccess::exists(String::utf8(p_name));
	if (p_flags == SQLITE_ACCESS_READWRITE) {
		*r_result = exists && !SQLiteVFS::is_read_only_path(p_name);
	} else {
		*r_result = exists;
	}
	return SQLITE_OK;
}

int _full_pathname(sqlite3_vfs *p_vfs, const char *p_name, int p_out_size, char *r_out) {
	// Paths stay as given: FileAccess resolves res://, user:// and path prefixes.
	const size_t length = strlen(p_name);
	if ((int64_t)length >= p_out_size) {
		return SQLITE_CANTOPEN;
	}
	memcpy(r_out, p_name, length + 1);
	return SQLITE_OK;
}

int _randomness(sqlite3_vfs *p_vfs, int p_bytes, char *r_out) {
	if (OS::get_singleton() && OS::get_singleton()->get_entropy(reinterpret_cast<uint8_t *>(r_out), p_bytes) == OK) {
		return p_bytes;
	}
	memset(r_out, 0, p_bytes);
	return p_bytes;
}

int _sleep(sqlite3_vfs *p_vfs, int p_microseconds) {
	if (p_microseconds > 0 && OS::get_singleton()) {
		OS::get_singleton()->delay_usec(p_microseconds);
	}
	return p_microseconds;
}

double _get_unix_time() {
	return OS::get_singleton() ? OS::get_singleton()->get_unix_time() : 0.0;
}

int _current_time(sqlite3_vfs *p_vfs, double *r_time) {
	// Julian day number.
	*r_time = _get_unix_time() / 86400.0 + 2440587.5;
	return SQLITE_OK;
}

int _current_time_int64(sqlite3_vfs *p_vfs, sqlite3_int64 *r_time) {
	// Julian day number times 86400000.
	*r_time = (sqlite3_int64)(_get_unix_time() * 1000.0) + (sqlite3_int64)210866760000000LL;
	return SQLITE_OK;
}

int _get_last_error(sqlite3_vfs *p_vfs, int p_size, char *r_out) {
	return 0;
}

sqlite3_vfs godot_vfs = {
	2, // iVersion
	(int)sizeof(GodotSQLiteFile),
	4096, // mxPathname
	nullptr,
	SQLiteVFS::NAME,
	nullptr,
	_open,
	_delete,
	_access,
	_full_pathname,
	nullptr, // xDlOpen: SQLITE_OMIT_LOAD_EXTENSION.
	nullptr,
	nullptr,
	nullptr,
	_randomness,
	_sleep,
	_current_time,
	_get_last_error,
	_current_time_int64,
	nullptr,
	nullptr,
	nullptr,
};

#if SQLITE_THREADSAFE
// With SQLITE_OS_OTHER, SQLite only ships a no-op mutex: provide real ones.
struct GodotSQLiteMutex {
	Mutex mutex;
};

constexpr int STATIC_MUTEX_COUNT = SQLITE_MUTEX_STATIC_VFS3 + 1;
GodotSQLiteMutex static_mutexes[STATIC_MUTEX_COUNT];

int _mutex_init() {
	return SQLITE_OK;
}

int _mutex_end() {
	return SQLITE_OK;
}

sqlite3_mutex *_mutex_alloc(int p_type) {
	if (p_type == SQLITE_MUTEX_FAST || p_type == SQLITE_MUTEX_RECURSIVE) {
		// Godot's Mutex is recursive, which also satisfies SQLITE_MUTEX_FAST.
		return reinterpret_cast<sqlite3_mutex *>(memnew(GodotSQLiteMutex));
	}
	if (p_type < 0 || p_type >= STATIC_MUTEX_COUNT) {
		return nullptr;
	}
	return reinterpret_cast<sqlite3_mutex *>(&static_mutexes[p_type]);
}

void _mutex_free(sqlite3_mutex *p_mutex) {
	GodotSQLiteMutex *mutex = reinterpret_cast<GodotSQLiteMutex *>(p_mutex);
	if (mutex < static_mutexes || mutex >= static_mutexes + STATIC_MUTEX_COUNT) {
		memdelete(mutex);
	}
}

void _mutex_enter(sqlite3_mutex *p_mutex) {
	reinterpret_cast<GodotSQLiteMutex *>(p_mutex)->mutex.lock();
}

int _mutex_try(sqlite3_mutex *p_mutex) {
	return reinterpret_cast<GodotSQLiteMutex *>(p_mutex)->mutex.try_lock() ? SQLITE_OK : SQLITE_BUSY;
}

void _mutex_leave(sqlite3_mutex *p_mutex) {
	reinterpret_cast<GodotSQLiteMutex *>(p_mutex)->mutex.unlock();
}

const sqlite3_mutex_methods mutex_methods = {
	_mutex_init,
	_mutex_end,
	_mutex_alloc,
	_mutex_free,
	_mutex_enter,
	_mutex_try,
	_mutex_leave,
	nullptr, // xMutexHeld: only used by SQLITE_DEBUG builds.
	nullptr,
};
#endif // SQLITE_THREADSAFE

bool initialized = false;

} // namespace

// Called by sqlite3_initialize() and sqlite3_shutdown() (SQLITE_OS_OTHER).
extern "C" int sqlite3_os_init(void) {
	return sqlite3_vfs_register(&godot_vfs, 1);
}

extern "C" int sqlite3_os_end(void) {
	return sqlite3_vfs_unregister(&godot_vfs);
}

bool SQLiteVFS::initialize() {
	if (initialized) {
		return true;
	}
#if SQLITE_THREADSAFE
	if (sqlite3_config(SQLITE_CONFIG_MUTEX, &mutex_methods) != SQLITE_OK) {
		return false;
	}
#endif
	initialized = sqlite3_initialize() == SQLITE_OK;
	return initialized;
}

void SQLiteVFS::finalize() {
	if (!initialized) {
		return;
	}
	sqlite3_shutdown();
	initialized = false;
}

bool SQLiteVFS::is_read_only_path(const char *p_path) {
	return p_path && strncmp(p_path, "res://", 6) == 0;
}
