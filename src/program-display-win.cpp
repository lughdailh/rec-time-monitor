// Windows-only: hands OBS the widget's HWND so it can create a display
// surface inside it. Mirrors program-display-mac.mm's NSView version.
//
// UNTESTED: written without a Windows machine to build/run it on (see
// README.md). The general approach matches how obs-studio's own Qt display
// widgets embed a window handle, but this file has not actually been
// compiled or exercised — treat the first build on Windows as a debugging
// session, not a known-good drop-in.
#include "program-display.hpp"

#include <obs.h>

#ifndef _WIN32
#error "program-display-win.cpp is Windows-only"
#endif

#include <windows.h>

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
	info.window.hwnd = reinterpret_cast<HWND>(winId());

	display_ = obs_display_create(&info, 0);
	if (display_)
		obs_display_add_draw_callback(display_, DrawCallback, this);
}
