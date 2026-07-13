#pragma once

#include <QImage>
#include <QMutex>
#include <QWidget>

struct obs_display;
typedef struct obs_display obs_display_t;
struct gs_texture;
typedef struct gs_texture gs_texture_t;

// Embeds a native OBS display surface inside a Qt widget and renders the
// final mixed Program output (obs_render_main_texture) into it, the same
// texture the recording/streaming outputs consume, then draws the REC/LIVE
// badge (see badge-image.hpp) as the last thing in that same draw callback.
// Because the badge is composited into this same GPU frame rather than being
// a separate Qt window layered on top, it is always correctly on top and in
// sync, and it never reaches the texture recording/streaming outputs
// consume: that texture is read (obs_render_main_texture) before the badge
// is drawn on top of *this* display's own, separate output.
class OBSProgramDisplay : public QWidget {
	Q_OBJECT

public:
	explicit OBSProgramDisplay(QWidget *parent = nullptr);
	~OBSProgramDisplay() override;

	QPaintEngine *paintEngine() const override { return nullptr; }

	// Safe to call from the Qt UI thread; picked up and uploaded to the GPU
	// from the OBS video thread on the next frame. alarmActive requests the
	// pulsing border in alertColorHex (the time-threshold check and which
	// configured alert currently applies are both resolved by the caller,
	// which already tracks TimeTracker state and Settings on the UI thread).
	void SetOverlayState(const QImage &badgeImage, bool alarmActive, const QString &alertColorHex);

protected:
	void showEvent(QShowEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	void CreateDisplay();
	void DestroyDisplay();
	static void DrawCallback(void *data, uint32_t cx, uint32_t cy);
	void Draw(uint32_t cx, uint32_t cy);
	void UpdateOverlayStateIfNeeded();
	void DrawAlertBorder(double canvasW, double canvasH);

	obs_display_t *display_ = nullptr;

	QMutex overlayMutex_;
	QImage pendingBadgeImage_;
	bool pendingAlarmActive_ = false;
	QString pendingAlertColorHex_;
	bool overlayStateDirty_ = false;

	// Video-thread-only state, touched only inside Draw()/DrawCallback().
	gs_texture_t *badgeTexture_ = nullptr;
	int badgeTextureWidth_ = 0;
	int badgeTextureHeight_ = 0;
	bool alarmActive_ = false;
	QString alertColorHex_ = "#ff3b30";
};
