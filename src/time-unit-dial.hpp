#pragma once

#include <QWidget>

// A single big two-digit number (hours, minutes or seconds) that IS the
// control: no spin buttons, no text entry box. Clicking the top half
// increments, the bottom half decrements (wrapping). A faint highlight and
// chevron appear on hover as the only affordance, so the digits themselves
// stay the dominant visual element.
class TimeUnitDial : public QWidget {
	Q_OBJECT

public:
	explicit TimeUnitDial(int maxValue, QWidget *parent = nullptr);

	int value() const { return value_; }
	void setValue(int v);

	QSize sizeHint() const override;

signals:
	void valueChanged(int value);

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;

private:
	int value_ = 0;
	int maxValue_;
	bool hovering_ = false;
	bool hoverTopHalf_ = true;
};
