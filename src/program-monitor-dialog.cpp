#include "program-monitor-dialog.hpp"
#include "badge-image.hpp"
#include "program-display.hpp"
#include "message-image.hpp"
#include "settings.hpp"
#include "time-tracker.hpp"

#include <QAction>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QWindow>
#include <QTimer>
#include <QScreen>

namespace {

QString FormatHMS(int64_t ms)
{
	const int64_t totalSeconds = ms / 1000;
	const int64_t h = totalSeconds / 3600;
	const int64_t m = (totalSeconds % 3600) / 60;
	const int64_t s = totalSeconds % 60;
	return QString("%1:%2:%3").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}

} // namespace

ProgramMonitorDialog::ProgramMonitorDialog(QWidget *parent) : QWidget(parent, Qt::Window)
{
	setWindowTitle("Monitor de Programa");
	// The embedded obs_display (OBSProgramDisplay) is tied to this window's
	// native NSView. Closing used to just hide the window (WA_DeleteOnClose
	// was false, to let "Open Monitor" reuse it), but a hidden window can
	// still tear down its native view while the display's draw callback
	// keeps running on OBS's video thread every frame, presenting into a
	// stale surface — a use-after-free that crashed the whole app. Deleting
	// on close destroys OBSProgramDisplay (which tears down the obs_display
	// in its destructor) at the same moment the window actually goes away;
	// plugin-main.cpp's QPointer already recreates a fresh window next time
	// "Open Monitor" is used.
	setAttribute(Qt::WA_DeleteOnClose, true);
	resize(960, 540);

	display_ = new OBSProgramDisplay(this);
	display_->setGeometry(rect());

	timer_ = new QTimer(this);
	connect(timer_, &QTimer::timeout, this, &ProgramMonitorDialog::UpdateOverlay);
	timer_->start(200);

	UpdateOverlay();
}

void ProgramMonitorDialog::ShowQuickMessage(const QString &message, int durationMs)
{
	quickMessage_ = message.trimmed();
	quickMessageUntil_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(durationMs);
	UpdateOverlay();
}

void ProgramMonitorDialog::ShowOnScreen(int screenIndex, bool fullscreen)
{
	const auto screens = QGuiApplication::screens();
	QScreen *screen = nullptr;
	if (screenIndex >= 0 && screenIndex < screens.size())
		screen = screens.at(screenIndex);
	if (!screen)
		screen = QGuiApplication::primaryScreen();

	if (!screen)
		return;

	if (!isVisible())
		show();

	move(screen->geometry().topLeft());
	resize(screen->geometry().size());
	if (windowHandle())
		windowHandle()->setScreen(screen);

	if (fullscreen)
		showFullScreen();
	else
		showNormal();

	raise();
	activateWindow();
}

void ProgramMonitorDialog::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	display_->setGeometry(rect());
}

void ProgramMonitorDialog::keyPressEvent(QKeyEvent *event)
{
	// The window normally opens fullscreen on an external "confidence
	// monitor" with no keyboard/mouse handy on that screen, and there is no
	// window chrome to close it from once it is up. Escape is the one
	// universally-known way out, whichever screen it ends up on.
	if (event->key() == Qt::Key_Escape) {
		close();
		return;
	}
	QWidget::keyPressEvent(event);
}

void ProgramMonitorDialog::UpdateOverlay()
{
	const TimeTracker::State state = TimeTracker::Instance().GetState();
	const auto now = std::chrono::steady_clock::now();

	QString colorHex = "#8e8e93";
	QString badgeText = "STANDBY";
	int64_t ms = 0;
	bool active = false;

	if (state.recording) {
		colorHex = "#ff3b30";
		badgeText = state.recordingPaused ? "PAUSA" : "REC";
		ms = state.recordingElapsedMs;
		active = !state.recordingPaused;
	} else if (state.streaming) {
		colorHex = "#ff9500";
		badgeText = "LIVE";
		ms = state.streamingElapsedMs;
		active = true;
	}

	// Of every configured checkpoint already reached, the most recently
	// passed one (largest threshold <= elapsed) wins, so the border color
	// switches from one checkpoint's color to the next as time passes.
	bool alarmActive = false;
	QString alertColorHex = "#ff3b30";
	int bestSeconds = -1;
	if (active) {
		const int64_t elapsedSeconds = ms / 1000;
		for (const AlertEntry &alert : Settings::Instance().Alerts()) {
			if (elapsedSeconds >= alert.seconds && alert.seconds > bestSeconds) {
				bestSeconds = alert.seconds;
				alertColorHex = alert.colorHex;
				alarmActive = true;
			}
		}
	}

	const double scale = Settings::Instance().ScalePercent() / 100.0;
	display_->SetOverlayState(RenderBadgeImage(colorHex, badgeText, FormatHMS(ms), scale), alarmActive,
				  alertColorHex);

	// Only push a new message texture on activate/deactivate, not on every
	// 200ms tick: the text and scale don't change while a message is up, so
	// re-rendering and re-uploading it every tick would just be wasted GPU
	// work for the whole duration it's shown.
	const bool quickMessageActive = !quickMessage_.isEmpty() && now < quickMessageUntil_;
	if (quickMessageActive != quickMessageWasActive_) {
		display_->SetMessageOverlay(quickMessageActive ? RenderMessageImage(quickMessage_, scale) : QImage(),
					    quickMessageActive);
		quickMessageWasActive_ = quickMessageActive;
	}
}
