/**************************************************************************/
/*  xr_settings.h                                                         */
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

#include "modules/modules_enabled.gen.h" // For openxr.

#if defined(MODULE_OPENXR_ENABLED) && !defined(XR_DISABLED)

#include "core/io/resource.h"
#include "core/variant/binder_common.h"

// Data of the `xr` settings domain (#30): OpenXR foveation, render target size, display refresh rate, Vulkan VRS,
// CPU and GPU performance levels, hand motion ranges and the game-side hand tracking switch.
// XRSettingsServer pushes the values to the OpenXR interface of XRServer. Only built with the openxr module.
class XRSettings : public Resource {
	GDCLASS(XRSettings, Resource);

public:
	// The values of OpenXRInterface.foveation_level.
	enum FoveationLevel {
		FOVEATION_LEVEL_OFF,
		FOVEATION_LEVEL_LOW,
		FOVEATION_LEVEL_MEDIUM,
		FOVEATION_LEVEL_HIGH,
	};

	// OpenXRInterface.PerfSettingsLevel, shifted by one: RUNTIME_DEFAULT is never pushed.
	enum PerformanceLevel {
		PERFORMANCE_LEVEL_RUNTIME_DEFAULT,
		PERFORMANCE_LEVEL_POWER_SAVINGS,
		PERFORMANCE_LEVEL_SUSTAINED_LOW,
		PERFORMANCE_LEVEL_SUSTAINED_HIGH,
		PERFORMANCE_LEVEL_BOOST,
	};

	// The values of OpenXRInterface.HandMotionRange.
	enum HandMotionRange {
		HAND_MOTION_RANGE_UNOBSTRUCTED,
		HAND_MOTION_RANGE_CONFORM_TO_CONTROLLER,
	};

private:
	FoveationLevel foveation_level = FOVEATION_LEVEL_OFF;
	bool foveation_dynamic = false;
	bool foveation_with_subsampled_images = true;
	float render_target_size_multiplier = 1.0;
	float display_refresh_rate = 0.0;
	float vrs_min_radius = 20.0;
	float vrs_strength = 1.0;
	PerformanceLevel cpu_level = PERFORMANCE_LEVEL_RUNTIME_DEFAULT;
	PerformanceLevel gpu_level = PERFORMANCE_LEVEL_RUNTIME_DEFAULT;
	HandMotionRange left_hand_motion_range = HAND_MOTION_RANGE_UNOBSTRUCTED;
	HandMotionRange right_hand_motion_range = HAND_MOTION_RANGE_UNOBSTRUCTED;
	bool hand_tracking_enabled = true;

protected:
	static void _bind_methods();

public:
	void set_foveation_level(FoveationLevel p_level);
	FoveationLevel get_foveation_level() const;
	void set_foveation_dynamic(bool p_enabled);
	bool is_foveation_dynamic() const;
	void set_foveation_with_subsampled_images(bool p_enabled);
	bool is_foveation_with_subsampled_images() const;
	void set_render_target_size_multiplier(float p_multiplier);
	float get_render_target_size_multiplier() const;
	void set_display_refresh_rate(float p_rate);
	float get_display_refresh_rate() const;
	void set_vrs_min_radius(float p_radius);
	float get_vrs_min_radius() const;
	void set_vrs_strength(float p_strength);
	float get_vrs_strength() const;
	void set_cpu_level(PerformanceLevel p_level);
	PerformanceLevel get_cpu_level() const;
	void set_gpu_level(PerformanceLevel p_level);
	PerformanceLevel get_gpu_level() const;
	void set_left_hand_motion_range(HandMotionRange p_range);
	HandMotionRange get_left_hand_motion_range() const;
	void set_right_hand_motion_range(HandMotionRange p_range);
	HandMotionRange get_right_hand_motion_range() const;
	void set_hand_tracking_enabled(bool p_enabled);
	bool is_hand_tracking_enabled() const;
};

VARIANT_ENUM_CAST(XRSettings::FoveationLevel);
VARIANT_ENUM_CAST(XRSettings::PerformanceLevel);
VARIANT_ENUM_CAST(XRSettings::HandMotionRange);

#endif // MODULE_OPENXR_ENABLED && !XR_DISABLED
