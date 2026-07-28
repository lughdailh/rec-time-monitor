#include "settings-dialog.hpp"
#include "settings.hpp"

#include <QColorDialog>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QFrame>
#include <QFontDatabase>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QRegion>
#include <QSpinBox>
#include <QScrollArea>
#include <QScreen>
#include <QTabWidget>
#include <QVBoxLayout>
#include <obs-module.h>

#include <algorithm>
#include <functional>

namespace {

QPushButton *ColorSwatchButton(const QString &colorHex, QWidget *parent)
{
	auto *button = new QPushButton(parent);
	button->setFixedSize(26, 26);
	button->setMask(QRegion(0, 0, 26, 26, QRegion::Ellipse));
	button->setCursor(Qt::PointingHandCursor);
	button->setStyleSheet(
		QString("background-color: %1; border-radius: 13px; border: 1px solid rgba(255,255,255,70);")
			.arg(colorHex));
	return button;
}

QString FormatTimeCode(int totalSeconds)
{
	return QStringLiteral("%1:%2:%3")
		.arg(totalSeconds / 3600, 2, 10, QChar('0'))
		.arg((totalSeconds % 3600) / 60, 2, 10, QChar('0'))
		.arg(totalSeconds % 60, 2, 10, QChar('0'));
}

int ParseTimeCode(const QString &text)
{
	const QStringList parts = text.split(':');
	if (parts.size() != 3)
		return 0;
	return parts[0].toInt() * 3600 + parts[1].toInt() * 60 + parts[2].toInt();
}

class TimeCodeEdit : public QLineEdit {
public:
	TimeCodeEdit(int seconds, std::function<void(int)> changed, QWidget *parent)
		: QLineEdit(FormatTimeCode(seconds), parent),
		  seconds_(seconds),
		  changed_(std::move(changed))
	{
		setInputMask(QStringLiteral("00:00:00"));
		setAlignment(Qt::AlignCenter);
		setToolTip(obs_module_text("Settings.TimeCodeHelp"));
	}

	void CommitTypedValue()
	{
		seconds_ = std::clamp(ParseTimeCode(text()), 0, 99 * 3600 + 59 * 60 + 59);
		setText(FormatTimeCode(seconds_));
		changed_(seconds_);
	}

protected:
	void keyPressEvent(QKeyEvent *event) override
	{
		if (event->key() != Qt::Key_Up && event->key() != Qt::Key_Down) {
			QLineEdit::keyPressEvent(event);
			return;
		}

		if (!event->isAutoRepeat() || !holdTimer_.isValid())
			holdTimer_.start();

		const qint64 heldMs = holdTimer_.elapsed();
		const int step = heldMs < 1000 ? 1 : heldMs < 2200 ? 5 : heldMs < 3500 ? 10 : heldMs < 5000 ? 30 : 60;
		const int direction = event->key() == Qt::Key_Up ? 1 : -1;
		seconds_ = std::clamp(seconds_ + direction * step, 0, 99 * 3600 + 59 * 60 + 59);
		setText(FormatTimeCode(seconds_));
		changed_(seconds_);
		event->accept();
	}

	void keyReleaseEvent(QKeyEvent *event) override
	{
		if ((event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) && !event->isAutoRepeat())
			holdTimer_.invalidate();
		QLineEdit::keyReleaseEvent(event);
	}

private:
	int seconds_ = 0;
	std::function<void(int)> changed_;
	QElapsedTimer holdTimer_;
};

} // namespace

SettingsDialog::SettingsDialog(MonitorTargetCallback monitorTargetCallback, QWidget *parent)
	: QWidget(parent, Qt::Window),
	  monitorTargetCallback_(std::move(monitorTargetCallback))
{
	setWindowTitle(obs_module_text("Settings.Title"));
	setAttribute(Qt::WA_DeleteOnClose, false);

	auto *rootLayout = new QVBoxLayout(this);
	rootLayout->setContentsMargins(10, 10, 10, 10);

	auto *tabs = new QTabWidget(this);
	tabs->setDocumentMode(true);
	rootLayout->addWidget(tabs);

	auto *appearancePage = new QWidget(tabs);
	auto *appearanceLayout = new QVBoxLayout(appearancePage);
	appearanceLayout->setContentsMargins(10, 12, 10, 10);
	appearanceLayout->setSpacing(10);

	auto *outputGroup = new QGroupBox(obs_module_text("Settings.MonitorOutput"), appearancePage);
	auto *outputLayout = new QFormLayout(outputGroup);
	outputLayout->setVerticalSpacing(8);
	outputLayout->setHorizontalSpacing(10);
	outputLayout->setContentsMargins(12, 14, 12, 12);
	monitorTargetCombo_ = new QComboBox(outputGroup);
	outputLayout->addRow(obs_module_text("Settings.MonitorTarget"), monitorTargetCombo_);
	PopulateMonitorTargets();
	connect(monitorTargetCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
		if (index < 0)
			return;
		const QString target = monitorTargetCombo_->itemData(index).toString();
		Settings::Instance().SetMonitorTarget(target);
		if (monitorTargetCallback_)
			monitorTargetCallback_(target);
	});
	appearanceLayout->addWidget(outputGroup);

	auto *badgeGroup = new QGroupBox(obs_module_text("Settings.Timer"), appearancePage);
	auto *badgeLayout = new QFormLayout(badgeGroup);
	badgeLayout->setVerticalSpacing(10);
	badgeLayout->setHorizontalSpacing(10);
	badgeLayout->setContentsMargins(12, 14, 12, 12);

	cornerCombo_ = new QComboBox(badgeGroup);
	cornerCombo_->addItem(obs_module_text("Position.TopRight"), static_cast<int>(Settings::BadgeCorner::TopRight));
	cornerCombo_->addItem(obs_module_text("Position.TopLeft"), static_cast<int>(Settings::BadgeCorner::TopLeft));
	cornerCombo_->addItem(obs_module_text("Position.TopCenter"),
			      static_cast<int>(Settings::BadgeCorner::TopCenter));
	cornerCombo_->addItem(obs_module_text("Position.BottomRight"),
			      static_cast<int>(Settings::BadgeCorner::BottomRight));
	cornerCombo_->addItem(obs_module_text("Position.BottomLeft"),
			      static_cast<int>(Settings::BadgeCorner::BottomLeft));
	cornerCombo_->addItem(obs_module_text("Position.BottomCenter"),
			      static_cast<int>(Settings::BadgeCorner::BottomCenter));
	{
		const int current = static_cast<int>(Settings::Instance().Corner());
		const int index = cornerCombo_->findData(current);
		cornerCombo_->setCurrentIndex(index >= 0 ? index : 0);
	}
	badgeLayout->addRow(obs_module_text("Settings.Position"), cornerCombo_);
	connect(cornerCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
		Settings::Instance().SetCorner(
			static_cast<Settings::BadgeCorner>(cornerCombo_->itemData(index).toInt()));
	});

	scaleSpin_ = new QSpinBox(badgeGroup);
	scaleSpin_->setRange(100, 300);
	scaleSpin_->setSuffix(" %");
	scaleSpin_->setSingleStep(10);
	scaleSpin_->setValue(Settings::Instance().ScalePercent());
	badgeLayout->addRow(obs_module_text("Settings.Size"), scaleSpin_);
	connect(scaleSpin_, &QSpinBox::valueChanged, this,
		[](int percent) { Settings::Instance().SetScalePercent(percent); });

	appearanceLayout->addWidget(badgeGroup);
	appearanceLayout->addStretch();
	tabs->addTab(appearancePage, obs_module_text("Settings.AppearanceTab"));

	auto *marksPage = new QWidget(tabs);
	auto *marksPageLayout = new QVBoxLayout(marksPage);
	marksPageLayout->setContentsMargins(10, 12, 10, 10);

	auto *alertsGroup = new QGroupBox(obs_module_text("Settings.Marks"), marksPage);
	auto *alertsGroupLayout = new QVBoxLayout(alertsGroup);
	alertsGroupLayout->setSpacing(8);
	alertsGroupLayout->setContentsMargins(10, 12, 10, 10);
	auto *marksHelp = new QLabel(obs_module_text("Settings.MarksHelp"), alertsGroup);
	marksHelp->setWordWrap(true);
	alertsGroupLayout->addWidget(marksHelp);

	auto *marksContents = new QWidget(alertsGroup);
	alertsLayout_ = new QVBoxLayout(marksContents);
	alertsLayout_->setContentsMargins(0, 0, 0, 0);
	auto *scrollArea = new QScrollArea(alertsGroup);
	scrollArea->setWidgetResizable(true);
	scrollArea->setFrameShape(QFrame::NoFrame);
	scrollArea->setWidget(marksContents);
	alertsGroupLayout->addWidget(scrollArea, 1);

	auto *addButton = new QPushButton(obs_module_text("Settings.AddMark"), alertsGroup);
	addButton->setMinimumHeight(32);
	connect(addButton, &QPushButton::clicked, this, [this]() {
		Settings::Instance().AddAlert(60, "#ff3b30");
		RebuildAlertRows();
	});
	alertsGroupLayout->addWidget(addButton);

	marksPageLayout->addWidget(alertsGroup);
	tabs->addTab(marksPage, obs_module_text("Settings.MarksTab"));

	setLayout(rootLayout);

	RebuildAlertRows();

	resize(460, 400);

	// A new installation starts with the practical, non-disruptive floating
	// target. Persist it when the user first opens settings so subsequent
	// OBS launches can restore the same destination.
	if (Settings::Instance().MonitorTarget().isEmpty()) {
		const QString target = QStringLiteral("floating");
		Settings::Instance().SetMonitorTarget(target);
		if (monitorTargetCallback_)
			monitorTargetCallback_(target);
	}
}

void SettingsDialog::PopulateMonitorTargets()
{
	const QString savedTarget = Settings::Instance().MonitorTarget();
	monitorTargetCombo_->clear();
	monitorTargetCombo_->addItem(obs_module_text("Settings.FloatingWindow"), QStringLiteral("floating"));

	for (QScreen *screen : QGuiApplication::screens()) {
		const QSize size = screen->geometry().size();
		const QString label =
			QStringLiteral("%1 — %2 × %3").arg(screen->name()).arg(size.width()).arg(size.height());
		monitorTargetCombo_->addItem(label, QStringLiteral("screen:") + screen->name());
	}

	const int savedIndex = monitorTargetCombo_->findData(savedTarget);
	monitorTargetCombo_->setCurrentIndex(savedIndex >= 0 ? savedIndex : 0);
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
		row->setObjectName(QStringLiteral("rtmMarkRow"));
		auto *rowLayout = new QVBoxLayout(row);
		rowLayout->setContentsMargins(6, 6, 6, 6);
		rowLayout->setSpacing(5);
		row->setStyleSheet("#rtmMarkRow { background: rgba(255,255,255,7); border-radius: 7px; }");

		auto *topLayout = new QHBoxLayout();
		topLayout->setContentsMargins(0, 0, 0, 0);
		topLayout->setSpacing(8);

		QPushButton *colorButton = ColorSwatchButton(entry.colorHex, row);
		connect(colorButton, &QPushButton::clicked, this,
			[this, index, colorButton, current = entry.colorHex]() mutable {
				const QColor chosen = QColorDialog::getColor(QColor(current), this,
									     obs_module_text("Settings.ChooseColor"));
				if (!chosen.isValid())
					return;
				current = chosen.name();
				Settings::Instance().SetAlertColor(index, current);
				colorButton->setStyleSheet(QString("background-color: %1; border-radius: 13px; "
								   "border: 1px solid rgba(255,255,255,70);")
								   .arg(current));
			});
		topLayout->addWidget(colorButton);

		auto *timeCode = new TimeCodeEdit(
			entry.seconds, [index](int seconds) { Settings::Instance().SetAlertSeconds(index, seconds); },
			row);
		timeCode->setFixedWidth(178);
		timeCode->setMinimumHeight(42);
		QFont timeFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		timeFont.setPixelSize(26);
		timeFont.setWeight(QFont::Medium);
		timeCode->setFont(timeFont);
		timeCode->setStyleSheet("QLineEdit { padding: 5px 9px; border-radius: 7px; }");
		connect(timeCode, &QLineEdit::editingFinished, timeCode, &TimeCodeEdit::CommitTypedValue);
		topLayout->addWidget(timeCode);

		auto *durationSpin = new QSpinBox(row);
		durationSpin->setRange(1, 600);
		durationSpin->setSuffix(QStringLiteral(" s"));
		durationSpin->setValue(entry.durationSeconds);
		durationSpin->setFixedWidth(72);
		durationSpin->setToolTip(obs_module_text("Settings.MarkDuration"));
		connect(durationSpin, &QSpinBox::valueChanged, this,
			[index](int seconds) { Settings::Instance().SetAlertDuration(index, seconds); });
		topLayout->addWidget(durationSpin);
		topLayout->addStretch(1);

		auto *removeButton = new QPushButton(QStringLiteral("×"), row);
		removeButton->setFixedSize(30, 30);
		removeButton->setCursor(Qt::PointingHandCursor);
		removeButton->setToolTip(obs_module_text("Settings.DeleteMark"));
		removeButton->setStyleSheet(
			"QPushButton { color: #f2f2f2; font-size: 22px; font-weight: 600; padding: 0; }");
		connect(removeButton, &QPushButton::clicked, this, [this, index]() {
			Settings::Instance().RemoveAlert(index);
			RebuildAlertRows();
		});
		topLayout->addWidget(removeButton);
		rowLayout->addLayout(topLayout);

		auto *messageEdit = new QLineEdit(entry.message, row);
		messageEdit->setPlaceholderText(obs_module_text("Settings.MarkMessage"));
		messageEdit->setClearButtonEnabled(true);
		connect(messageEdit, &QLineEdit::editingFinished, this,
			[index, messageEdit]() { Settings::Instance().SetAlertMessage(index, messageEdit->text()); });
		rowLayout->addWidget(messageEdit);

		alertsLayout_->addWidget(row);
	}

	if (alerts.isEmpty())
		alertsLayout_->addWidget(new QLabel(obs_module_text("Settings.NoMarks"), this));
}

namespace {

QPointer<SettingsDialog> &SharedSettingsDialog()
{
	static QPointer<SettingsDialog> instance;
	return instance;
}

} // namespace

void PrepareSettingsDialog(QWidget *parent, SettingsDialog::MonitorTargetCallback monitorTargetCallback)
{
	QPointer<SettingsDialog> &instance = SharedSettingsDialog();

	if (!instance)
		instance = new SettingsDialog(std::move(monitorTargetCallback), parent);
}

void ShowSettingsDialog(QWidget *parent, SettingsDialog::MonitorTargetCallback monitorTargetCallback)
{
	PrepareSettingsDialog(parent, std::move(monitorTargetCallback));
	QPointer<SettingsDialog> &instance = SharedSettingsDialog();

	instance->show();
	instance->raise();
	instance->activateWindow();
}
