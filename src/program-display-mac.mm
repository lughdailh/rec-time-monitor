// macOS-only: hands OBS the widget's NSView so it can create a display
// surface inside it. This is the one part of OBSProgramDisplay that can't be
// shared with Windows (program-display-win.cpp) — gs_init_data::window.view
// is an Objective-C `id`, which only exists when this file is compiled as
// Objective-C++ (the .mm extension).
#include "program-display.hpp"

#include <obs.h>

#ifndef __APPLE__
#error "program-display-mac.mm is macOS-only"
#endif

#include <Cocoa/Cocoa.h>

void OBSProgramDisplay::CreateDisplay()
{
	if (display_ || !winId() || width() < 1 || height() < 1)
		return;

	const qreal pixelRatio = devicePixelRatioF();

	gs_init_data info = {};
	info.cx = static_cast<uint32_t>(width() * pixelRatio);
	info.cy = static_cast<uint32_t>(height() * pixelRatio);
	info.format = GS_BGRA;
	info.zsformat = GS_ZS_NONE;
	// gs_init_data::window.view is typed `id` on macOS; under ARC a __bridge
	// cast is required to hand over the NSView pointer without a retain.
	info.window.view = (__bridge id)(void *)winId();

	display_ = obs_display_create(&info, 0);
	if (display_)
		obs_display_add_draw_callback(display_, DrawCallback, this);
}
