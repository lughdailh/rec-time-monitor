#pragma once

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

protected:
	void resizeEvent(QResizeEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
	void UpdateOverlay();

private:
	OBSProgramDisplay *display_ = nullptr;
	QTimer *timer_ = nullptr;
};
