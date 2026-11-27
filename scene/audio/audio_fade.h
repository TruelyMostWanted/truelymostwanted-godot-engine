/**************************************************************************/
/*  audio_fade.h                                                          */
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

#include "core/math/math_funcs.h"
#include "core/typedefs.h"

// One fade of a gain or a volume over time, shared by AudioMixerServer and the AudioStreamPlayer nodes.
// The owner advances it on the main thread and applies the value to the playback volume; AudioServer
// ramps that volume over each mix block, so the fade has no audible steps.
struct AudioFade {
	// Same values as AudioMixerServer::FadeCurve, the bound enum.
	enum Curve {
		CURVE_LINEAR,
		CURVE_EASE_IN,
		CURVE_EASE_OUT,
		CURVE_EQUAL_POWER,
	};

	bool active = false;
	float from = 0.0f;
	float to = 0.0f;
	double time = 0.0;
	double elapsed = 0.0;
	int curve = CURVE_LINEAR;

	// The value at the current elapsed time.
	float get_value() const {
		const float t = time > 0.0 ? (float)CLAMP(elapsed / time, 0.0, 1.0) : 1.0f;
		float shape = t;
		switch (curve) {
			case CURVE_EASE_IN: {
				shape = t * t;
			} break;
			case CURVE_EASE_OUT: {
				shape = 1.0f - (1.0f - t) * (1.0f - t);
			} break;
			case CURVE_EQUAL_POWER: {
				// Rising gains follow sin, falling gains follow cos, so the summed power of a cross-fade stays 1.
				const float angle = t * (float)Math::PI * 0.5f;
				shape = to >= from ? Math::sin(angle) : 1.0f - Math::cos(angle);
			} break;
			default:
				break;
		}
		return from + (to - from) * shape;
	}

	// Starts a fade from p_from to p_to. A time of 0 or less ends it at once (inactive, value p_to).
	void start(float p_from, float p_to, double p_time, int p_curve) {
		active = p_time > 0.0;
		from = active ? p_from : p_to;
		to = p_to;
		time = MAX(p_time, 0.0);
		elapsed = 0.0;
		curve = p_curve;
	}

	// Advances an active fade by p_delta seconds and writes the new value to r_value.
	// Returns true when the fade has just finished.
	bool advance(double p_delta, float &r_value) {
		if (!active) {
			return false;
		}
		elapsed += p_delta;
		if (elapsed >= time) {
			active = false;
			r_value = to;
			return true;
		}
		r_value = get_value();
		return false;
	}
};
