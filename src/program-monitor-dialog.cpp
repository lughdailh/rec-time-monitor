#include "program-monitor-dialog.hpp"
#include "badge-image.hpp"
#include "program-display.hpp"
#include "settings-dialog.hpp"
#include "settings.hpp"
#include "time-tracker.hpp"

#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>
#include <QResizeEvent>
#include <QTimer>

namespace {

QString FormatHMS(int64_t ms)
{
	const int64_t totalSeconds = ms / 1000;
	const int64_t h = totalSeconds / 3600;
	const int64_t m = (totalSeconds % 3600) / 60;
	const int64_t s = totalSeconds % 60;
	return QString("%1:%2:%3")
		.arg(h, 2, 10, QChar('0'))
		.arg(m, 2, 10, QChar('0'))
		.arg(s, 2, 10, QChar('0'));
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

void ProgramMonitorDialog::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	display_->setGeometry(rect());
}

void ProgramMonitorDialog::contextMenuEvent(QContextMenuEvent *event)
{
	QMenu menu(this);
	QAction *settingsAction = menu.addAction("Configuració del Monitor de Programa...");
	menu.addSeparator();
	QAction *closeAction = menu.addAction("Tanca aquesta finestra");

	QAction *chosen = menu.exec(event->globalPos());
	if (chosen == settingsAction)
		ShowSettingsDialog(this);
	else if (chosen == closeAction)
		close();
}

void ProgramMonitorDialog::UpdateOverlay()
{
	const TimeTracker::State state = TimeTracker::Instance().GetState();

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
	display_->SetOverlayState(RenderBadgeImage(colorHex, badgeText, FormatHMS(ms), scale), alarmActive, alertColorHex);
}
