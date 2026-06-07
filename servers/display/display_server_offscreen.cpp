/**************************************************************************/
/*  display_server_offscreen.cpp                                          */
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

#include "display_server_offscreen.h"

#include "core/input/input.h"
#include "core/input/input_event.h"
#include "servers/display/native_menu.h"
#include "servers/rendering/dummy/rasterizer_dummy.h"

#if defined(RD_ENABLED)
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#if defined(VULKAN_ENABLED)
#include "drivers/vulkan/rendering_context_driver_vulkan.h"
#endif
#endif

DisplayServer *DisplayServerOffscreen::create_func(const String &p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error &r_error) {
	r_error = OK;
	DisplayServerOffscreen *ds = memnew(DisplayServerOffscreen());
	ds->rendering_driver = p_rendering_driver;
	ds->main_window.size = p_resolution;

	if (ds->rendering_driver == "dummy") {
		RasterizerDummy::make_current();
	} else if (ds->rendering_driver == "vulkan") {
#if defined(RD_ENABLED)
		RendererCompositorRD::make_current();
#endif
	}

	if (ds->rendering_driver != "dummy") {
#if defined(RD_ENABLED)
#if defined(VULKAN_ENABLED)
		if (ds->rendering_driver == "vulkan") {
			ds->rendering_context = memnew(RenderingContextDriverVulkan);
		}
#endif
		if (ds->rendering_context) {
			if (ds->rendering_context->initialize() != OK) {
				memdelete(ds->rendering_context);
				ds->rendering_context = nullptr;
			}
		}

		if (ds->rendering_context) {
			ds->rendering_device = memnew(RenderingDevice);
			if (ds->rendering_device->initialize(ds->rendering_context, DisplayServerEnums::INVALID_WINDOW_ID) != OK) {
				memdelete(ds->rendering_device);
				ds->rendering_device = nullptr;
				memdelete(ds->rendering_context);
				ds->rendering_context = nullptr;
			}
		}
#endif
	}

	if (ds->rendering_driver != "dummy" && ds->rendering_context == nullptr) {
		r_error = ERR_CANT_CREATE;
		memdelete(ds);
		return nullptr;
	}

	return ds;
}

void DisplayServerOffscreen::_dispatch_input_events(const Ref<InputEvent> &p_event) {
	static_cast<DisplayServerOffscreen *>(get_singleton())->_dispatch_input_event(p_event);
}

void DisplayServerOffscreen::_dispatch_input_event(const Ref<InputEvent> &p_event) {
	if (input_event_callback.is_valid()) {
		input_event_callback.call(p_event);
	}
}

void DisplayServerOffscreen::process_events() {
	Input::get_singleton()->flush_buffered_events();
}

DisplayServerOffscreen::DisplayServerOffscreen() {
	native_menu = memnew(NativeMenu);
	Input::get_singleton()->set_event_dispatch_function(_dispatch_input_events);
}

DisplayServerOffscreen::~DisplayServerOffscreen() {
	if (native_menu) {
		memdelete(native_menu);
		native_menu = nullptr;
	}
#if defined(RD_ENABLED)
	if (rendering_device) {
		if (rendering_device->get_frames_drawn() > 0) {
			rendering_device->sync();
		}
		memdelete(rendering_device);
		rendering_device = nullptr;
	}
	if (rendering_context) {
		// Ensure any virtual window resources are cleaned up.
		rendering_context->window_destroy(DisplayServerEnums::MAIN_WINDOW_ID);
		memdelete(rendering_context);
		rendering_context = nullptr;
	}
#endif
}
