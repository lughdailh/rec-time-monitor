#pragma once

#include <chrono>
#include <QWidget>

class QTimer;
class OBSProgramDisplay;

// Auxiliary top-level window (like an OBS Projector) showing the mixed
// Program output with a REC/LIVE timer badge painted on top, entirely
// outside OBS's render graph, so it is never part of any recording/stream.
class ProgramMonitorDialog : public QWidget {
	Q_OBJECT

public:
	explicit ProgramMonitorDialog(QWidget *parent = nullptr);
	void ShowQuickMessage(const QString &message, int durationMs = 5000);
	void ShowOnScreen(int screenIndex, bool fullscreen = true);

protected:
	void resizeEvent(QResizeEvent *event) override;

private slots:
	void UpdateOverlay();

private:
	OBSProgramDisplay *display_ = nullptr;
	QTimer *timer_ = nullptr;
	QString quickMessage_;
	std::chrono::steady_clock::time_point quickMessageUntil_;
};
