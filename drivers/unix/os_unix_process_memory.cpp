/**************************************************************************/
/*  os_unix_process_memory.cpp                                            */
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

// Process memory of the running process for every platform whose OS class
// derives from OS_Unix: Linux/BSD, Android, macOS, iOS, visionOS and Web.

#include "drivers/unix/os_unix.h"

#ifdef UNIX_ENABLED

#if defined(__APPLE__)
#include <mach/mach.h>
#include <malloc/malloc.h>
#elif defined(WEB_ENABLED)
#include <emscripten/heap.h>
#include <malloc.h>
#else
#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>
#if defined(__GLIBC__) || defined(__ANDROID__)
#include <malloc.h>
#define GODOT_HAS_MALLINFO
#endif
#endif

#if defined(__APPLE__)

// Reads the VM counters of the own task. `r_has_footprint` is false on systems
// that do not report `phys_footprint` (TASK_VM_INFO revision 0).
static bool _get_task_vm_info(task_vm_info_data_t &r_info, bool &r_has_footprint) {
	r_info = {};
	mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
	if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&r_info, &count) != KERN_SUCCESS) {
		return false;
	}
	r_has_footprint = count >= TASK_VM_INFO_REV1_COUNT;
	return true;
}

uint64_t OS_Unix::get_process_memory_usage() const {
	task_vm_info_data_t info;
	bool has_footprint = false;
	if (!_get_task_vm_info(info, has_footprint)) {
		return 0;
	}
	// The physical footprint is what Xcode shows and what the iOS/visionOS memory limit is checked against.
	return has_footprint ? (uint64_t)info.phys_footprint : (uint64_t)info.resident_size;
}

Dictionary OS_Unix::get_process_memory_info() const {
	Dictionary meminfo;

	meminfo["resident"] = -1;
	meminfo["private"] = -1;
	meminfo["peak"] = -1;
	meminfo["heap"] = -1;

	task_vm_info_data_t info;
	bool has_footprint = false;
	if (_get_task_vm_info(info, has_footprint)) {
		meminfo["resident"] = (int64_t)info.resident_size;
		meminfo["peak"] = (int64_t)info.resident_size_peak;
		if (has_footprint) {
			meminfo["private"] = (int64_t)info.phys_footprint;
		}
	}

	malloc_statistics_t stats = {};
	malloc_zone_statistics(nullptr, &stats); // All zones.
	if (stats.size_in_use > 0) {
		meminfo["heap"] = (int64_t)stats.size_in_use;
	}

	return meminfo;
}

#elif defined(WEB_ENABLED)

// WebAssembly linear memory: it only grows, so its current size is also its peak.

uint64_t OS_Unix::get_process_memory_usage() const {
	return (uint64_t)emscripten_get_heap_size();
}

Dictionary OS_Unix::get_process_memory_info() const {
	Dictionary meminfo;

	const int64_t wasm_memory = (int64_t)emscripten_get_heap_size();
	meminfo["resident"] = wasm_memory > 0 ? wasm_memory : -1;
	meminfo["private"] = -1;
	meminfo["peak"] = wasm_memory > 0 ? wasm_memory : -1;

	// The fields are `int`; wasm32 memory is at most 4 GiB, so read them as unsigned.
	const struct mallinfo mi = mallinfo();
	meminfo["heap"] = (int64_t)(uint32_t)mi.uordblks;

	return meminfo;
}

#else // Linux, BSD, Android.

// Reads `resident` and `shared` (in bytes) from /proc/self/statm.
// Plain open/read/close without allocations: a few microseconds per call.
static bool _read_proc_statm(uint64_t &r_resident, uint64_t &r_shared) {
	const int fd = open("/proc/self/statm", O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		return false;
	}
	char buf[128];
	const ssize_t len = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (len <= 0) {
		return false;
	}
	buf[len] = '\0';

	// Format: "size resident shared text lib data dt", in pages.
	uint64_t values[3] = {};
	int index = 0;
	const char *c = buf;
	while (index < 3) {
		while (*c == ' ') {
			c++;
		}
		if (*c < '0' || *c > '9') {
			return false;
		}
		uint64_t value = 0;
		while (*c >= '0' && *c <= '9') {
			value = value * 10 + (uint64_t)(*c - '0');
			c++;
		}
		values[index++] = value;
	}

	static const uint64_t page_size = (uint64_t)sysconf(_SC_PAGESIZE);
	r_resident = values[1] * page_size;
	r_shared = values[2] * page_size;
	return true;
}

uint64_t OS_Unix::get_process_memory_usage() const {
	uint64_t resident = 0;
	uint64_t shared = 0;
	if (!_read_proc_statm(resident, shared)) {
		return 0;
	}
	return resident;
}

Dictionary OS_Unix::get_process_memory_info() const {
	Dictionary meminfo;

	meminfo["resident"] = -1;
	meminfo["private"] = -1;
	meminfo["peak"] = -1;
	meminfo["heap"] = -1;

	uint64_t resident = 0;
	uint64_t shared = 0;
	if (_read_proc_statm(resident, shared)) {
		meminfo["resident"] = (int64_t)resident;
		// Resident memory that is not file-backed or shared: the anonymous memory of this process.
		meminfo["private"] = (int64_t)(resident >= shared ? resident - shared : 0);
	}

	struct rusage usage = {};
	if (getrusage(RUSAGE_SELF, &usage) == 0 && usage.ru_maxrss > 0) {
		meminfo["peak"] = (int64_t)usage.ru_maxrss * 1024; // Kilobytes on Linux, Android and the BSDs.
	}

#ifdef GODOT_HAS_MALLINFO
#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
	const struct mallinfo2 mi = mallinfo2();
	meminfo["heap"] = (int64_t)mi.uordblks;
#elif defined(__GLIBC__)
	// glibc < 2.33 only has `int` fields.
	const struct mallinfo mi = mallinfo();
	meminfo["heap"] = (int64_t)(uint32_t)mi.uordblks;
#else
	// Bionic uses `size_t` fields.
	const struct mallinfo mi = mallinfo();
	meminfo["heap"] = (int64_t)mi.uordblks;
#endif
#endif // GODOT_HAS_MALLINFO

	return meminfo;
}

#endif

#endif // UNIX_ENABLED
