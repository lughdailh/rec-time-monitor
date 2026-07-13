// Platform-independent parts of OBSProgramDisplay. CreateDisplay() (the only
// bit that has to hand OBS a native window handle) lives in
// program-display-mac.mm (NSView, needs Objective-C++) or
// program-display-win.cpp (HWND) instead.
#include "program-display.hpp"
#include "settings.hpp"

#include <obs.h>
#include <util/platform.h>

#include <QColor>
#include <QMutexLocker>
#include <QResizeEvent>
#include <QShowEvent>

#include <algorithm>
#include <cmath>
#include <cstdint>

OBSProgramDisplay::OBSProgramDisplay(QWidget *parent) : QWidget(parent)
{
	setAttribute(Qt::WA_NativeWindow);
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAutoFillBackground(false);
}

OBSProgramDisplay::~OBSProgramDisplay()
{
	DestroyDisplay();

	if (badgeTexture_) {
		obs_enter_graphics();
		gs_texture_destroy(badgeTexture_);
		obs_leave_graphics();
		badgeTexture_ = nullptr;
	}
}

void OBSProgramDisplay::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	CreateDisplay();
}

void OBSProgramDisplay::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);

	if (!display_) {
		CreateDisplay();
		return;
	}

	const qreal pixelRatio = devicePixelRatioF();
	obs_display_resize(display_, static_cast<uint32_t>(width() * pixelRatio),
			    static_cast<uint32_t>(height() * pixelRatio));
}

void OBSProgramDisplay::DestroyDisplay()
{
	if (!display_)
		return;

	obs_display_remove_draw_callback(display_, DrawCallback, this);
	obs_display_destroy(display_);
	display_ = nullptr;
}

void OBSProgramDisplay::SetOverlayState(const QImage &badgeImage, bool alarmActive, const QString &alertColorHex)
{
	QMutexLocker locker(&overlayMutex_);
	pendingBadgeImage_ = badgeImage;
	pendingAlarmActive_ = alarmActive;
	pendingAlertColorHex_ = alertColorHex;
	overlayStateDirty_ = true;
}

void OBSProgramDisplay::UpdateOverlayStateIfNeeded()
{
	// Runs on the OBS video thread (called from Draw(), itself called from
	// the draw callback), where issuing gs_ calls directly is safe.
	QImage image;
	bool haveNewImage;
	{
		QMutexLocker locker(&overlayMutex_);
		alarmActive_ = pendingAlarmActive_;
		if (!pendingAlertColorHex_.isEmpty())
			alertColorHex_ = pendingAlertColorHex_;
		haveNewImage = overlayStateDirty_;
		if (haveNewImage)
			image = pendingBadgeImage_;
		overlayStateDirty_ = false;
	}

	if (!haveNewImage)
		return;

	if (badgeTexture_) {
		gs_texture_destroy(badgeTexture_);
		badgeTexture_ = nullptr;
	}

	if (image.isNull())
		return;

	// QImage::Format_ARGB32's in-memory byte order (B, G, R, A on
	// little-endian) matches GS_BGRA directly, no conversion needed.
	image = image.convertToFormat(QImage::Format_ARGB32);
	const uint8_t *data = image.constBits();
	badgeTexture_ = gs_texture_create(static_cast<uint32_t>(image.width()), static_cast<uint32_t>(image.height()),
					   GS_BGRA, 1, &data, 0);
	badgeTextureWidth_ = image.width();
	badgeTextureHeight_ = image.height();
}

void OBSProgramDisplay::DrawCallback(void *data, uint32_t cx, uint32_t cy)
{
	static_cast<OBSProgramDisplay *>(data)->Draw(cx, cy);
}

void OBSProgramDisplay::DrawAlertBorder(double canvasW, double canvasH)
{
	const double thickness = std::max(6.0, std::min(canvasW, canvasH) * 0.02);

	// Pulses between ~30% and ~90% opacity, roughly once per second.
	const double t = static_cast<double>(os_gettime_ns()) / 1e9;
	const double alpha = 0.6 + 0.3 * std::sin(t * 2.0 * M_PI);

	const QColor qcolor(alertColorHex_);

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *colorParam = gs_effect_get_param_by_name(solid, "color");

	struct vec4 color;
	vec4_set(&color, static_cast<float>(qcolor.redF()), static_cast<float>(qcolor.greenF()),
		  static_cast<float>(qcolor.blueF()), static_cast<float>(alpha));
	gs_effect_set_vec4(colorParam, &color);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

	const struct {
		double x, y, w, h;
	} strips[4] = {
		{0.0, 0.0, canvasW, thickness},                     // top
		{0.0, canvasH - thickness, canvasW, thickness},      // bottom
		{0.0, 0.0, thickness, canvasH},                      // left
		{canvasW - thickness, 0.0, thickness, canvasH},      // right
	};

	while (gs_effect_loop(solid, "Solid")) {
		for (const auto &s : strips) {
			gs_matrix_push();
			gs_matrix_translate3f(static_cast<float>(s.x), static_cast<float>(s.y), 0.0f);
			gs_draw_sprite(nullptr, 0, static_cast<uint32_t>(s.w), static_cast<uint32_t>(s.h));
			gs_matrix_pop();
		}
	}

	gs_blend_state_pop();
}

void OBSProgramDisplay::Draw(uint32_t cx, uint32_t cy)
{
	if (cx == 0 || cy == 0)
		return;

	obs_video_info ovi;
	if (!obs_get_video_info(&ovi))
		return;

	const double targetW = static_cast<double>(ovi.base_width);
	const double targetH = static_cast<double>(ovi.base_height);
	if (targetW <= 0.0 || targetH <= 0.0)
		return;

	const double scale = std::min(static_cast<double>(cx) / targetW, static_cast<double>(cy) / targetH);
	const int scaledW = static_cast<int>(targetW * scale);
	const int scaledH = static_cast<int>(targetH * scale);
	const int offsetX = (static_cast<int>(cx) - scaledW) / 2;
	const int offsetY = (static_cast<int>(cy) - scaledH) / 2;

	gs_viewport_push();
	gs_projection_push();

	gs_ortho(0.0f, static_cast<float>(targetW), 0.0f, static_cast<float>(targetH), -100.0f, 100.0f);
	gs_set_viewport(offsetX, offsetY, scaledW, scaledH);

	// The final mixed Program texture: exactly what recording/streaming
	// outputs consume, regardless of Studio Mode.
	obs_render_main_texture();

	// Everything below is composited into this same GPU frame, on top of the
	// video, as the very next draw calls: always correctly on top and in
	// sync, and it never touches the texture the line above reads (that read
	// already happened), so none of it can ever end up in a recording/stream.
	UpdateOverlayStateIfNeeded();

	if (alarmActive_)
		DrawAlertBorder(targetW, targetH);

	if (badgeTexture_) {
		constexpr float margin = 40.0f;
		float badgeX = margin;
		float badgeY = margin;

		switch (Settings::Instance().Corner()) {
		case Settings::BadgeCorner::TopLeft:
			badgeX = margin;
			badgeY = margin;
			break;
		case Settings::BadgeCorner::TopRight:
			badgeX = static_cast<float>(targetW) - static_cast<float>(badgeTextureWidth_) - margin;
			badgeY = margin;
			break;
		case Settings::BadgeCorner::BottomLeft:
			badgeX = margin;
			badgeY = static_cast<float>(targetH) - static_cast<float>(badgeTextureHeight_) - margin;
			break;
		case Settings::BadgeCorner::BottomRight:
			badgeX = static_cast<float>(targetW) - static_cast<float>(badgeTextureWidth_) - margin;
			badgeY = static_cast<float>(targetH) - static_cast<float>(badgeTextureHeight_) - margin;
			break;
		case Settings::BadgeCorner::TopCenter:
			badgeX = (static_cast<float>(targetW) - static_cast<float>(badgeTextureWidth_)) / 2.0f;
			badgeY = margin;
			break;
		case Settings::BadgeCorner::BottomCenter:
			badgeX = (static_cast<float>(targetW) - static_cast<float>(badgeTextureWidth_)) / 2.0f;
			badgeY = static_cast<float>(targetH) - static_cast<float>(badgeTextureHeight_) - margin;
			break;
		}

		gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		gs_eparam_t *imageParam = gs_effect_get_param_by_name(effect, "image");
		gs_effect_set_texture(imageParam, badgeTexture_);

		gs_blend_state_push();
		gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

		while (gs_effect_loop(effect, "Draw")) {
			gs_matrix_push();
			gs_matrix_translate3f(badgeX, badgeY, 0.0f);
			gs_draw_sprite(badgeTexture_, 0, static_cast<uint32_t>(badgeTextureWidth_),
					static_cast<uint32_t>(badgeTextureHeight_));
			gs_matrix_pop();
		}

		gs_blend_state_pop();
	}

	gs_projection_pop();
	gs_viewport_pop();
}
