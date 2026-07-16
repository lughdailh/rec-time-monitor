#include "settings-dialog.hpp"
#include "big-time-editor.hpp"
#include "settings.hpp"

#include <QColorDialog>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

QPushButton *ColorSwatchButton(const QString &colorHex, QWidget *parent)
{
	auto *button = new QPushButton(parent);
	button->setFixedSize(28, 28);
	button->setCursor(Qt::PointingHandCursor);
	button->setStyleSheet(QString("background-color: %1; border-radius: 6px; border: 1px solid rgba(255,255,255,40);")
				      .arg(colorHex));
	return button;
}

} // namespace

SettingsDialog::SettingsDialog(QWidget *parent) : QWidget(parent, Qt::Window)
{
	setWindowTitle("Configuració del Monitor de Programa");
	setAttribute(Qt::WA_DeleteOnClose, false);

	auto *rootLayout = new QVBoxLayout(this);

	auto *alertsGroup = new QGroupBox("Avisos de temps", this);
	auto *alertsGroupLayout = new QVBoxLayout(alertsGroup);
	alertsGroupLayout->setSpacing(12);
	alertsGroupLayout->setContentsMargins(18, 18, 18, 18);
	alertsGroupLayout->addWidget(
		new QLabel("En arribar a cada temps, el marc de la imatge es posa del color indicat.", alertsGroup));

	alertsLayout_ = new QVBoxLayout();
	alertsGroupLayout->addLayout(alertsLayout_);

	auto *addButton = new QPushButton("+ Afegeix un avís", alertsGroup);
	addButton->setMinimumHeight(44);
	connect(addButton, &QPushButton::clicked, this, [this]() {
		Settings::Instance().AddAlert(60, "#ff3b30");
		RebuildAlertRows();
	});
	alertsGroupLayout->addWidget(addButton);

	rootLayout->addWidget(alertsGroup);

	auto *badgeGroup = new QGroupBox("Cronòmetre", this);
	auto *badgeLayout = new QFormLayout(badgeGroup);
	badgeLayout->setVerticalSpacing(16);
	badgeLayout->setHorizontalSpacing(16);
	badgeLayout->setContentsMargins(18, 18, 18, 18);

	cornerCombo_ = new QComboBox(badgeGroup);
	cornerCombo_->addItem("Superior dreta", static_cast<int>(Settings::BadgeCorner::TopRight));
	cornerCombo_->addItem("Superior esquerra", static_cast<int>(Settings::BadgeCorner::TopLeft));
	cornerCombo_->addItem("Superior centre", static_cast<int>(Settings::BadgeCorner::TopCenter));
	cornerCombo_->addItem("Inferior dreta", static_cast<int>(Settings::BadgeCorner::BottomRight));
	cornerCombo_->addItem("Inferior esquerra", static_cast<int>(Settings::BadgeCorner::BottomLeft));
	cornerCombo_->addItem("Inferior centre", static_cast<int>(Settings::BadgeCorner::BottomCenter));
	{
		const int current = static_cast<int>(Settings::Instance().Corner());
		const int index = cornerCombo_->findData(current);
		cornerCombo_->setCurrentIndex(index >= 0 ? index : 0);
	}
	badgeLayout->addRow("Posició:", cornerCombo_);
	connect(cornerCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
		Settings::Instance().SetCorner(static_cast<Settings::BadgeCorner>(cornerCombo_->itemData(index).toInt()));
	});

	scaleSpin_ = new QSpinBox(badgeGroup);
	scaleSpin_->setRange(100, 300);
	scaleSpin_->setSuffix(" %");
	scaleSpin_->setSingleStep(10);
	scaleSpin_->setValue(Settings::Instance().ScalePercent());
	badgeLayout->addRow("Mida:", scaleSpin_);
	connect(scaleSpin_, &QSpinBox::valueChanged, this, [](int percent) { Settings::Instance().SetScalePercent(percent); });

	rootLayout->addWidget(badgeGroup);

	setLayout(rootLayout);

	RebuildAlertRows();

	resize(420, sizeHint().height());
}

void SettingsDialog::RebuildAlertRows()
{
	QLayoutItem *item;
	while ((item = alertsLayout_->takeAt(0)) != nullptr) {
		delete item->widget();
		delete item;
	}

	const QVector<AlertEntry> alerts = Settings::Instance().Alerts();

	for (int index = 0; index < alerts.size(); index++) {
		const AlertEntry &entry = alerts[index];

		auto *row = new QWidget(this);
		auto *rowLayout = new QHBoxLayout(row);
		rowLayout->setContentsMargins(0, 4, 0, 4);
		rowLayout->setSpacing(12);

		QPushButton *colorButton = ColorSwatchButton(entry.colorHex, row);
		connect(colorButton, &QPushButton::clicked, this,
			[this, index, colorButton, current = entry.colorHex]() mutable {
				const QColor chosen = QColorDialog::getColor(QColor(current), this, "Tria un color");
				if (!chosen.isValid())
					return;
				current = chosen.name();
				Settings::Instance().SetAlertColor(index, current);
				colorButton->setStyleSheet(QString("background-color: %1; border-radius: 6px; "
								    "border: 1px solid rgba(255,255,255,40);")
								    .arg(current));
			});
		rowLayout->addWidget(colorButton);

		auto *timeEditor = new BigTimeEditor(row);
		timeEditor->setTotalSeconds(entry.seconds);
		timeEditor->setMinimumWidth(420);
		connect(timeEditor, &BigTimeEditor::secondsChanged, this,
			[index](int seconds) { Settings::Instance().SetAlertSeconds(index, seconds); });
		rowLayout->addWidget(timeEditor, 1);

		auto *removeButton = new QPushButton("✕", row);
		removeButton->setFixedSize(32, 32);
		removeButton->setCursor(Qt::PointingHandCursor);
		removeButton->setStyleSheet("font-size: 18px; font-weight: 700;");
		connect(removeButton, &QPushButton::clicked, this, [this, index]() {
			Settings::Instance().RemoveAlert(index);
			RebuildAlertRows();
		});
		rowLayout->addWidget(removeButton);

		alertsLayout_->addWidget(row);
	}

	if (alerts.isEmpty())
		alertsLayout_->addWidget(new QLabel("Cap avís configurat.", this));
}

void ShowSettingsDialog(QWidget *parent)
{
	static QPointer<SettingsDialog> instance;

	if (!instance)
		instance = new SettingsDialog(parent);

	instance->show();
	instance->raise();
	instance->activateWindow();
}
