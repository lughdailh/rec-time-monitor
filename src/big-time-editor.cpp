#include "big-time-editor.hpp"
#include "time-unit-dial.hpp"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>

namespace {

QLabel *ColonLabel(QWidget *parent)
{
	auto *label = new QLabel(QStringLiteral(":"), parent);
	QFont f = label->font();
	f.setPixelSize(34);
	f.setBold(true);
	label->setFont(f);
	label->setStyleSheet("color: rgba(255, 255, 255, 180);");
	label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	return label;
}

} // namespace

BigTimeEditor::BigTimeEditor(QWidget *parent) : QWidget(parent)
{
	hours_ = new TimeUnitDial(99, this);
	minutes_ = new TimeUnitDial(59, this);
	seconds_ = new TimeUnitDial(59, this);

	for (TimeUnitDial *dial : {hours_, minutes_, seconds_})
		dial->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	setMinimumHeight(146);

	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(10);
	layout->addWidget(hours_, 1);
	layout->addWidget(ColonLabel(this), 0);
	layout->addWidget(minutes_, 1);
	layout->addWidget(ColonLabel(this), 0);
	layout->addWidget(seconds_, 1);

	connect(hours_, &TimeUnitDial::valueChanged, this, [this](int) { emit secondsChanged(totalSeconds()); });
	connect(minutes_, &TimeUnitDial::valueChanged, this, [this](int) { emit secondsChanged(totalSeconds()); });
	connect(seconds_, &TimeUnitDial::valueChanged, this, [this](int) { emit secondsChanged(totalSeconds()); });
}

int BigTimeEditor::totalSeconds() const
{
	return hours_->value() * 3600 + minutes_->value() * 60 + seconds_->value();
}

void BigTimeEditor::setTotalSeconds(int seconds)
{
	if (seconds < 0)
		seconds = 0;

	hours_->setValue(seconds / 3600);
	minutes_->setValue((seconds % 3600) / 60);
	seconds_->setValue(seconds % 60);
}
