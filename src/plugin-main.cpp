#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <QFrame>
#include <QSignalBlocker>
#include <QRegularExpression>
#include <QSpinBox>
#include <memory>
#include <QSplitter>
#include <QTableWidget>
#include <QHeaderView>
#include <QSettings>
#include <QFont>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QPointer>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>

#include <algorithm>

#include "program-monitor-dialog.hpp"
#include "quick-message-dialog.hpp"
#include "settings.hpp"
#include "settings-dialog.hpp"
#include "time-tracker.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {

QPointer<ProgramMonitorDialog> g_dialog;
QPointer<QWidget> g_dock;
QPointer<QuickMessageDialog> g_messageDialog;
QPointer<QPushButton> g_monitorToggle;
QPointer<QPushButton> g_monitorLock;

void OpenPluginMonitor(void *);
bool OpenPluginMonitorTarget(const QString &target);
void UpdateMonitorControls();
void PrepareQuickMessageDialog();

class MonitorDock : public QWidget {
public:
	explicit MonitorDock(QWidget *parent) : QWidget(parent) {}

	void SetTypographyTargets(QLabel *elapsed, QLabel *nextTitle, QLabel *nextMark, QPushButton *message)
	{
		elapsed_ = elapsed;
		nextTitle_ = nextTitle;
		nextMark_ = nextMark;
		message_ = message;
		UpdateTypography();
	}

protected:
	void showEvent(QShowEvent *event) override
	{
		QWidget::showEvent(event);
		QTimer::singleShot(0, this, [this]() { UpdateTypography(); });
	}

	void resizeEvent(QResizeEvent *event) override
	{
		QWidget::resizeEvent(event);
		UpdateTypography();
	}

private:
	void UpdateTypography()
	{
		const int usableWidth = std::max(245, (elapsed_ && elapsed_->parentWidget() ? elapsed_->parentWidget()->width() : width()) - 40);

		auto applyFont = [](QWidget *widget, int pixelSize, QFont::Weight weight) {
			if (!widget)
				return;
			QFont font = widget->font(); // native UI font: SF Pro on macOS, Segoe UI on Windows
			font.setPixelSize(pixelSize);
			font.setWeight(weight);
			widget->setFont(font);
			// OBS themes apply their own font rules after widgets are docked.
			// A local size/weight rule wins that cascade without fixing a
			// family, so the native platform font remains in use.
			widget->setStyleSheet(QString("font-size: %1px; font-weight: %2;")
						      .arg(pixelSize)
						      .arg(static_cast<int>(weight)));
		};

		applyFont(elapsed_, std::clamp(static_cast<int>(usableWidth * 0.20), 46, 78), QFont::Light);
		applyFont(nextTitle_, std::clamp(static_cast<int>(usableWidth * 0.052), 15, 24), QFont::Normal);
		applyFont(nextMark_, std::clamp(static_cast<int>(usableWidth * 0.12), 30, 50), QFont::Light);
		applyFont(message_, std::clamp(static_cast<int>(usableWidth * 0.058), 16, 24), QFont::Normal);

		if (elapsed_)
			elapsed_->setMinimumHeight(std::clamp(static_cast<int>(usableWidth * 0.23), 72, 105));
		if (message_)
			message_->setMinimumHeight(std::clamp(static_cast<int>(usableWidth * 0.12), 40, 52));
	}

	QLabel *elapsed_ = nullptr;
	QLabel *nextTitle_ = nullptr;
	QLabel *nextMark_ = nullptr;
	QPushButton *message_ = nullptr;
};

QWidget *MainWindow()
{
	return static_cast<QWidget *>(obs_frontend_get_main_window());
}

QString FormatHMS(int64_t totalSeconds)
{
	const int64_t hours = totalSeconds / 3600;
	const int64_t minutes = (totalSeconds % 3600) / 60;
	const int64_t seconds = totalSeconds % 60;
	return QString("%1:%2:%3")
		.arg(hours, 2, 10, QChar('0'))
		.arg(minutes, 2, 10, QChar('0'))
		.arg(seconds, 2, 10, QChar('0'));
}

void FrontendEventCallback(enum obs_frontend_event event, void *)
{
	TimeTracker::Instance().HandleFrontendEvent(static_cast<int>(event));
}

void OpenPluginMonitor(void *)
{
	const QString target = Settings::Instance().MonitorTarget();
	const bool opened = OpenPluginMonitorTarget(target) || OpenPluginMonitorTarget(QStringLiteral("floating"));
	Settings::Instance().SetMonitorEnabled(opened);
	UpdateMonitorControls();
}

bool OpenPluginMonitorTarget(const QString &target)
{
	int screenIndex = -1;
	bool fullscreen = false;

	if (target.startsWith(QStringLiteral("screen:"))) {
		const QString screenName = target.mid(QStringLiteral("screen:").size());
		const auto screens = QGuiApplication::screens();
		for (int index = 0; index < screens.size(); ++index) {
			if (screens.at(index)->name() == screenName) {
				screenIndex = index;
				fullscreen = true;
				break;
			}
		}
		if (screenIndex < 0)
			return false;
	} else if (target != QStringLiteral("floating")) {
		return false;
	}

	if (!g_dialog) {
		QWidget *mainWindow = static_cast<QWidget *>(obs_frontend_get_main_window());
		g_dialog = new ProgramMonitorDialog(mainWindow);
		QObject::connect(g_dialog, &ProgramMonitorDialog::ClosedByUser, MainWindow(), []() {
			Settings::Instance().SetMonitorEnabled(false);
			UpdateMonitorControls();
		});
	}
	g_dialog->SetCloseLocked(Settings::Instance().MonitorLocked());
	g_dialog->ShowOnScreen(screenIndex, fullscreen);
	return true;
}

void PrepareQuickMessageDialog()
{
	if (!g_messageDialog) {
		g_messageDialog = new QuickMessageDialog(MainWindow());
		QObject::connect(g_messageDialog, &QuickMessageDialog::SendRequested, MainWindow(),
				 [](const QString &text, int durationMs) {
					 const QString trimmed = text.trimmed();
					 if (trimmed.isEmpty())
						 return;
					 if (!g_dialog)
						 OpenPluginMonitor(nullptr);
					 if (g_dialog)
						 g_dialog->ShowQuickMessage(trimmed, durationMs);
				 });
	}
}

void OpenQuickMessageDialog(void *)
{
	PrepareQuickMessageDialog();
	g_messageDialog->show();
	g_messageDialog->raise();
	g_messageDialog->activateWindow();
}

void OpenPreferencesDialog(void *)
{
	obs_log(LOG_DEBUG, "opening settings window");
	ShowSettingsDialog(MainWindow(), [](const QString &target) {
		const bool opened = OpenPluginMonitorTarget(target);
		Settings::Instance().SetMonitorEnabled(opened);
		UpdateMonitorControls();
	});
}

void UpdateMonitorControls()
{
	const bool enabled = Settings::Instance().MonitorEnabled();
	const bool locked = Settings::Instance().MonitorLocked();

	if (g_monitorToggle) {
		g_monitorToggle->setChecked(enabled);
		g_monitorToggle->setText(obs_module_text(enabled ? "Dock.MonitorOn" : "Dock.MonitorOff"));
		g_monitorToggle->setEnabled(!locked);
	}
	if (g_monitorLock) {
		g_monitorLock->setChecked(locked);
		g_monitorLock->setText(locked ? QStringLiteral("🔒") : QStringLiteral("🔓"));
		g_monitorLock->setToolTip(obs_module_text(locked ? "Dock.Unlock" : "Dock.Lock"));
	}
	if (g_dialog)
		g_dialog->SetCloseLocked(locked);
}


class DockMessageTable final : public QTableWidget {
public:
	using QTableWidget::QTableWidget;
	bool IsEditing() const { return state() == QAbstractItemView::EditingState; }
};

void SendDockMessage(const QString &text, int seconds)
{
	if (text.trimmed().isEmpty())
		return;
	if (!g_dialog)
		OpenPluginMonitor(nullptr);
	if (g_dialog)
		g_dialog->ShowQuickMessage(text.trimmed(), std::clamp(seconds, 1, 600) * 1000);
}

QWidget *CreateMessagesPanel(QSplitter *splitter)
{
	auto *panel = new QWidget(splitter);
	panel->setMinimumWidth(270);
	auto *layout = new QVBoxLayout(panel);
	layout->setContentsMargins(4, 8, 8, 8);
	auto *table = new DockMessageTable(panel);
	table->setColumnCount(4);
	table->setHorizontalHeaderLabels({QStringLiteral("#"), obs_module_text("Dock.TableTime"),
					  obs_module_text("Dock.TableMessage"), obs_module_text("Message.Send")});
	table->verticalHeader()->hide();
	table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
	table->horizontalHeader()->setMinimumSectionSize(28);
	table->setColumnWidth(0, 34);
	table->setColumnWidth(1, 92);
	table->setColumnWidth(2, 190);
	table->setColumnWidth(3, 75);
	table->setSelectionBehavior(QAbstractItemView::SelectRows);
	table->setSelectionMode(QAbstractItemView::SingleSelection);
	layout->addWidget(table, 1);
	auto *buttons = new QHBoxLayout;
	auto *add = new QPushButton(obs_module_text("Settings.AddMark"), panel);
	auto *remove = new QPushButton(obs_module_text("Settings.DeleteMark"), panel);
	buttons->addWidget(add);
	buttons->addWidget(remove);
	layout->addLayout(buttons);
	auto *duration = new QSpinBox(panel);
	duration->setRange(1, 600);
	duration->setValue(5);
	duration->setSuffix(QStringLiteral(" s"));
	auto *durationRow = new QHBoxLayout;
	durationRow->addWidget(new QLabel(obs_module_text("Message.Duration"), panel));
	durationRow->addWidget(duration);
	layout->addLayout(durationRow);

	auto presets = Settings::Instance().MessagePresets();
	if (presets.size() != 6) {
		presets.clear();
		for (const char *key : {"Message.Preset1", "Message.Preset2", "Message.Preset3",
					"Message.Preset4", "Message.Preset5", "Message.Preset6"})
			presets.append(obs_module_text(key));
		Settings::Instance().SetMessagePresets(presets);
	}

	auto signature = []() {
		QStringList result = Settings::Instance().MessagePresets();
		for (const auto &alert : Settings::Instance().Alerts()) {
			result << QString::number(alert.seconds) << alert.message
			       << QString::number(alert.durationSeconds) << alert.colorHex;
		}
		return result;
	};
	auto rebuild = [table, duration]() {
		const QSignalBlocker blocker(table);
		const int previousRow = table->currentRow();
		const auto messages = Settings::Instance().MessagePresets();
		const auto alerts = Settings::Instance().Alerts();
		const int presetCount = static_cast<int>(messages.size());
		const int count = presetCount + static_cast<int>(alerts.size());
		table->setRowCount(0);
		table->setRowCount(count);
		for (int row = 0; row < count; ++row) {
			const bool manual = row < presetCount;
			const int index = manual ? row : row - presetCount;
			auto *number = new QTableWidgetItem(QString::number(row + 1));
			number->setFlags(number->flags() & ~Qt::ItemIsEditable);
			table->setItem(row, 0, number);
			auto *time = new QTableWidgetItem(manual ? QStringLiteral("—") : FormatHMS(alerts[index].seconds));
			if (manual)
				time->setFlags(time->flags() & ~Qt::ItemIsEditable);
			table->setItem(row, 1, time);
			auto *message = new QTableWidgetItem(manual ? messages[index] : alerts[index].message);
			if (!manual)
				message->setToolTip(QStringLiteral("%1 s · %2").arg(alerts[index].durationSeconds).arg(alerts[index].colorHex));
			table->setItem(row, 2, message);
			auto *send = new QPushButton(obs_module_text("Message.Send"), table);
			send->setFocusPolicy(Qt::NoFocus);
			table->setCellWidget(row, 3, send);
			QObject::connect(send, &QPushButton::clicked, table, [table, row, manual, index, duration]() {
				const auto messagesNow = Settings::Instance().MessagePresets();
				const auto alertsNow = Settings::Instance().Alerts();
				if (manual && index < messagesNow.size())
					SendDockMessage(messagesNow[index], duration->value());
				else if (!manual && index < alertsNow.size() &&
					 table->item(row, 1)->text() == FormatHMS(alertsNow[index].seconds) &&
					 table->item(row, 2)->text() == alertsNow[index].message)
					SendDockMessage(alertsNow[index].message, alertsNow[index].durationSeconds);
			});
		}
		if (previousRow >= 0 && previousRow < count)
			table->selectRow(previousRow);
	};
	auto last = std::make_shared<QStringList>();
	auto refresh = [table, signature, rebuild, last]() {
		if (table->IsEditing())
			return;
		const auto current = signature();
		if (*last != current) {
			*last = current;
			rebuild();
		}
	};
	QObject::connect(table, &QTableWidget::itemChanged, panel,
			 [table, signature, last](QTableWidgetItem *item) {
		if (*last != signature())
			return; // Another editor changed the settings; refresh instead of editing a stale row.
		auto messages = Settings::Instance().MessagePresets();
		const int row = item->row();
		const int col = item->column();
		if (row < messages.size()) {
			if (col == 2 && !item->text().trimmed().isEmpty()) {
				messages[row] = item->text().trimmed();
				Settings::Instance().SetMessagePresets(messages);
			}
		} else {
			const int index = row - static_cast<int>(messages.size());
			const auto alerts = Settings::Instance().Alerts();
			if (index >= 0 && index < alerts.size()) {
				if (col == 2)
					Settings::Instance().SetAlertMessage(index, item->text());
				else if (col == 1) {
					const QRegularExpression valid(QStringLiteral("^[0-9]{1,2}:[0-5][0-9]:[0-5][0-9]$"));
					if (valid.match(item->text()).hasMatch()) {
						const auto parts = item->text().split(':');
						Settings::Instance().SetAlertSeconds(index, parts[0].toInt() * 3600 +
											     parts[1].toInt() * 60 + parts[2].toInt());
					}
				}
			}
		}
		// Normalize edits (including invalid timecodes) after the delegate closes.
		last->clear();
	});
	QObject::connect(add, &QPushButton::clicked, panel, [refresh]() {
		Settings::Instance().AddAlert(60, "#ff3b30");
		refresh();
	});
	QObject::connect(remove, &QPushButton::clicked, panel, [table, refresh, signature, last]() {
		if (*last != signature())
			return;
		const int index = table->currentRow() - static_cast<int>(Settings::Instance().MessagePresets().size());
		if (index >= 0)
			Settings::Instance().RemoveAlert(index);
		refresh();
	});
	auto *timer = new QTimer(panel);
	timer->setInterval(250);
	QObject::connect(timer, &QTimer::timeout, panel, refresh);
	refresh();
	timer->start();
	QSettings ui(QStringLiteral("REC Time Monitor"), QStringLiteral("Dock"));
	table->horizontalHeader()->restoreState(ui.value("columns").toByteArray());
	QObject::connect(table->horizontalHeader(), &QHeaderView::sectionResized, panel, [table]() {
		QSettings(QStringLiteral("REC Time Monitor"), QStringLiteral("Dock"))
			.setValue("columns", table->horizontalHeader()->saveState());
	});
	return panel;
}

QWidget *CreateMonitorDock(QWidget *parent)
{
	auto *dock = new MonitorDock(parent);
	dock->setObjectName("rec-time-monitor-dock");

	auto *root = new QHBoxLayout(dock);
	root->setContentsMargins(0, 0, 0, 0);
	auto *splitter = new QSplitter(Qt::Horizontal, dock);
	root->addWidget(splitter);
	auto *controls = new QWidget(splitter);
	controls->setMinimumWidth(260);
	auto *layout = new QVBoxLayout(controls);
	layout->setContentsMargins(10, 10, 10, 10);
	layout->setSpacing(7);

	auto *timerRow = new QWidget(dock);
	auto *timerLayout = new QHBoxLayout(timerRow);
	timerLayout->setContentsMargins(18, 8, 18, 8);
	timerLayout->setSpacing(14);

	auto *statusDot = new QLabel(timerRow);
	statusDot->setFixedSize(18, 18);
	timerLayout->addWidget(statusDot, 0, Qt::AlignVCenter);

	auto *elapsedLabel = new QLabel(QStringLiteral("00:00:00"), timerRow);
	elapsedLabel->setObjectName("rtmElapsed");
	elapsedLabel->setAlignment(Qt::AlignCenter);
	elapsedLabel->setMinimumHeight(76);
	elapsedLabel->setMaximumHeight(110);
	timerLayout->addWidget(elapsedLabel, 1);
	layout->addWidget(timerRow);

	auto *separator1 = new QFrame(dock);
	separator1->setFrameShape(QFrame::HLine);
	separator1->setObjectName("rtmSeparator");
	layout->addWidget(separator1);

	auto *nextRow = new QWidget(dock);
	auto *nextLayout = new QHBoxLayout(nextRow);
	nextLayout->setContentsMargins(18, 0, 18, 0);
	nextLayout->setSpacing(12);
	auto *nextTitle = new QLabel(obs_module_text("Dock.NextMark"), nextRow);
	nextTitle->setObjectName("rtmNextTitle");
	nextLayout->addWidget(nextTitle);
	auto *nextLabel = new QLabel(QStringLiteral("--:--:--"), nextRow);
	nextLabel->setObjectName("rtmNextMark");
	nextLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	nextLayout->addWidget(nextLabel, 1);
	layout->addWidget(nextRow);

	auto *separator2 = new QFrame(dock);
	separator2->setFrameShape(QFrame::HLine);
	separator2->setObjectName("rtmSeparator");
	layout->addWidget(separator2);

	auto *monitorControls = new QWidget(dock);
	auto *monitorControlsLayout = new QHBoxLayout(monitorControls);
	monitorControlsLayout->setContentsMargins(0, 0, 0, 0);
	monitorControlsLayout->setSpacing(6);
	g_monitorToggle = new QPushButton(monitorControls);
	g_monitorToggle->setCheckable(true);
	g_monitorToggle->setFixedHeight(34);
	monitorControlsLayout->addWidget(g_monitorToggle, 1);
	g_monitorLock = new QPushButton(monitorControls);
	g_monitorLock->setCheckable(true);
	g_monitorLock->setFixedSize(42, 34);
	monitorControlsLayout->addWidget(g_monitorLock);
	layout->addWidget(monitorControls);

	QObject::connect(g_monitorToggle, &QPushButton::clicked, dock, [](bool enabled) {
		if (enabled) {
			OpenPluginMonitor(nullptr);
		} else {
			Settings::Instance().SetMonitorEnabled(false);
			if (g_dialog)
				g_dialog->close();
			UpdateMonitorControls();
		}
	});
	QObject::connect(g_monitorLock, &QPushButton::clicked, dock, [](bool locked) {
		Settings::Instance().SetMonitorLocked(locked);
		UpdateMonitorControls();
	});
	UpdateMonitorControls();

	auto *messageButton = new QPushButton(obs_module_text("Dock.Message"), dock);
	messageButton->setObjectName("rtmMessageButton");
	messageButton->setMinimumHeight(40);
	messageButton->setMaximumHeight(52);
	QObject::connect(messageButton, &QPushButton::clicked, dock, []() { OpenQuickMessageDialog(nullptr); });
	layout->addWidget(messageButton);

	layout->addStretch(1);

	auto *separator3 = new QFrame(dock);
	separator3->setFrameShape(QFrame::HLine);
	separator3->setObjectName("rtmSeparator");
	layout->addWidget(separator3);

	auto *preferencesButton = new QPushButton(obs_module_text("Dock.Settings"), dock);
	preferencesButton->setObjectName("rtmSettingsButton");
	preferencesButton->setFixedHeight(32);
	QObject::connect(preferencesButton, &QPushButton::clicked, dock, []() { OpenPreferencesDialog(nullptr); });
	layout->addWidget(preferencesButton);

	splitter->addWidget(controls);
	splitter->addWidget(CreateMessagesPanel(splitter));
	splitter->setChildrenCollapsible(false);
	splitter->setSizes({280, 320});
	QSettings ui(QStringLiteral("REC Time Monitor"), QStringLiteral("Dock"));
	splitter->restoreState(ui.value("splitter").toByteArray());
	QObject::connect(splitter, &QSplitter::splitterMoved, dock, [splitter]() {
		QSettings(QStringLiteral("REC Time Monitor"), QStringLiteral("Dock"))
			.setValue("splitter", splitter->saveState());
	});
	QObject::connect(splitter, &QSplitter::splitterMoved, dock, [dock, elapsedLabel, nextTitle, nextLabel, messageButton]() {
		dock->SetTypographyTargets(elapsedLabel, nextTitle, nextLabel, messageButton);
	});
	dock->setMinimumWidth(540);
	dock->setStyleSheet("#rec-time-monitor-dock { background: #111111; }"
			    "#rtmElapsed { color: #f5f5f5; }"
			    "#rtmNextTitle { color: #f5f5f5; }"
			    "#rtmNextMark { color: #f5f5f5; }"
			    "#rtmSeparator { color: #3b3b3b; background: #3b3b3b; max-height: 2px; border: 0; }"
			    "#rtmMessageButton { background: #07820b; border: 1px solid #424242; border-radius: 10px;"
			    " color: white; padding: 5px 12px; }"
			    "#rtmMessageButton:hover { background: #0a9910; }"
			    "#rtmMessageButton:pressed { background: #056b09; }");
	dock->SetTypographyTargets(elapsedLabel, nextTitle, nextLabel, messageButton);

	auto *refreshTimer = new QTimer(dock);
	refreshTimer->setInterval(200);
	auto updateDock = [elapsedLabel, nextLabel, statusDot]() {
		const TimeTracker::State state = TimeTracker::Instance().GetState();
		int64_t elapsedMs = 0;
		QString dotColor = QStringLiteral("#666666");
		bool active = false;

		if (state.recording) {
			elapsedMs = state.recordingElapsedMs;
			dotColor = state.recordingPaused ? QStringLiteral("#d88b00") : QStringLiteral("#d7190b");
			active = true;
		} else if (state.streaming) {
			elapsedMs = state.streamingElapsedMs;
			dotColor = QStringLiteral("#ff9500");
			active = true;
		}

		elapsedLabel->setText(FormatHMS(elapsedMs / 1000));
		statusDot->setStyleSheet(QString("background: %1; border-radius: 9px;").arg(dotColor));

		int nextSeconds = -1;
		if (active) {
			const int elapsedSeconds = static_cast<int>(elapsedMs / 1000);
			for (const AlertEntry &mark : Settings::Instance().Alerts()) {
				if (mark.seconds > elapsedSeconds && (nextSeconds < 0 || mark.seconds < nextSeconds))
					nextSeconds = mark.seconds;
			}
			if (nextSeconds >= 0) {
				const int64_t remainingMs = static_cast<int64_t>(nextSeconds) * 1000 - elapsedMs;
				nextLabel->setText(FormatHMS(std::max<int64_t>(0, (remainingMs + 999) / 1000)));
			} else {
				nextLabel->setText(obs_module_text("Dock.NoNextMark"));
			}
		} else {
			nextLabel->setText(QStringLiteral("--:--:--"));
		}
	};
	QObject::connect(refreshTimer, &QTimer::timeout, dock, updateDock);
	updateDock();
	refreshTimer->start();

	return dock;
}

} // namespace

bool obs_module_load(void)
{
	TimeTracker::Instance().SyncCurrentState();
	obs_frontend_add_event_callback(FrontendEventCallback, nullptr);

	if (!g_dock) {
		g_dock = CreateMonitorDock(MainWindow());
		obs_frontend_add_dock_by_id("rec-time-monitor-dock", obs_module_text("Dock.Name"), g_dock);
	}

	const QString savedTarget = Settings::Instance().MonitorTarget();
	if (Settings::Instance().MonitorEnabled() && !savedTarget.isEmpty()) {
		// Let OBS finish constructing its own startup dialogs and docks before
		// raising a separate top-level monitor window.
		QTimer::singleShot(1800, MainWindow(), [savedTarget]() {
			if (!OpenPluginMonitorTarget(savedTarget)) {
				Settings::Instance().SetMonitorEnabled(false);
				UpdateMonitorControls();
				obs_log(LOG_INFO, "saved monitor target is not currently available");
			}
		});
	}

	QTimer::singleShot(2500, MainWindow(), []() {
		PrepareQuickMessageDialog();
		if (!Settings::Instance().MonitorTarget().isEmpty()) {
			PrepareSettingsDialog(MainWindow(), [](const QString &target) {
				const bool opened = OpenPluginMonitorTarget(target);
				Settings::Instance().SetMonitorEnabled(opened);
				UpdateMonitorControls();
			});
		}
	});

	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(FrontendEventCallback, nullptr);
	if (g_dock) {
		obs_frontend_remove_dock("rec-time-monitor-dock");
		g_dock = nullptr;
	}
	obs_log(LOG_INFO, "plugin unloaded");
}
