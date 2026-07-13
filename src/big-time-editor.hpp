#pragma once

#include <QWidget>

class TimeUnitDial;

// "00h 00m 00s" made of three TimeUnitDial widgets plus small unit-letter
// labels. The dials are given all the horizontal stretch so the digits — the
// only interactive part — dominate the width; the "h"/"m"/"s" labels take
// only as much space as their text needs.
class BigTimeEditor : public QWidget {
	Q_OBJECT

public:
	explicit BigTimeEditor(QWidget *parent = nullptr);

	int totalSeconds() const;
	void setTotalSeconds(int seconds);

signals:
	void secondsChanged(int totalSeconds);

private:
	TimeUnitDial *hours_ = nullptr;
	TimeUnitDial *minutes_ = nullptr;
	TimeUnitDial *seconds_ = nullptr;
};
