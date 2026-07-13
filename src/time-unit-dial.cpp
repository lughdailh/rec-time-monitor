#include "time-unit-dial.hpp"

#include <QMouseEvent>
#include <QPainter>

TimeUnitDial::TimeUnitDial(int maxValue, QWidget *parent) : QWidget(parent), maxValue_(maxValue)
{
	setCursor(Qt::PointingHandCursor);
	setMouseTracking(true);
}

QSize TimeUnitDial::sizeHint() const
{
	return QSize(90, 110);
}

void TimeUnitDial::setValue(int v)
{
	const int span = maxValue_ + 1;
	v = ((v % span) + span) % span;
	if (v == value_)
		return;
	value_ = v;
	update();
	emit valueChanged(value_);
}

void TimeUnitDial::mousePressEvent(QMouseEvent *event)
{
	if (event->button() != Qt::LeftButton)
		return;

	if (event->position().y() < height() / 2.0)
		setValue(value_ + 1);
	else
		setValue(value_ - 1);
}

void TimeUnitDial::mouseMoveEvent(QMouseEvent *event)
{
	const bool top = event->position().y() < height() / 2.0;
	if (top != hoverTopHalf_) {
		hoverTopHalf_ = top;
		update();
	}
}

void TimeUnitDial::enterEvent(QEnterEvent *)
{
	hovering_ = true;
	update();
}

void TimeUnitDial::leaveEvent(QEvent *)
{
	hovering_ = false;
	update();
}

void TimeUnitDial::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	if (hovering_) {
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(255, 255, 255, 16));
		p.drawRoundedRect(rect(), 12, 12);

		const QRectF half = hoverTopHalf_ ? QRectF(0, 0, width(), height() / 2.0)
						   : QRectF(0, height() / 2.0, width(), height() / 2.0);
		p.setBrush(QColor(255, 255, 255, 22));
		p.drawRect(half);
	}

	QFont numberFont = font();
	numberFont.setBold(true);
	numberFont.setPixelSize(static_cast<int>(height() * 0.62));
	p.setFont(numberFont);
	p.setPen(palette().color(QPalette::WindowText));
	p.drawText(rect(), Qt::AlignCenter, QString("%1").arg(value_, 2, 10, QChar('0')));

	if (hovering_) {
		QColor dim = palette().color(QPalette::WindowText);
		dim.setAlpha(100);
		p.setPen(dim);

		QFont chevronFont = font();
		chevronFont.setPixelSize(qMax(10, height() / 9));
		p.setFont(chevronFont);
		p.drawText(QRect(0, 4, width(), height() / 2 - 4), Qt::AlignHCenter | Qt::AlignTop, QStringLiteral("▲"));
		p.drawText(QRect(0, height() / 2, width(), height() / 2 - 4), Qt::AlignHCenter | Qt::AlignBottom,
			   QStringLiteral("▼"));
	}
}
