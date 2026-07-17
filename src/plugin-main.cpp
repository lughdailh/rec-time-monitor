#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <QComboBox>
#include <QGuiApplication>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include "program-monitor-dialog.hpp"
#include "quick-message-dialog.hpp"
#include "settings-dialog.hpp"
#include "time-tracker.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {

QPointer<ProgramMonitorDialog> g_dialog;
QPointer<QWidget> g_dock;
QPointer<QComboBox> g_monitorCombo;
QPointer<QuickMessageDialog> g_messageDialog;

void OpenPluginMonitor(void *);

QWidget *MainWindow()
{
	return static_cast<QWidget *>(obs_frontend_get_main_window());
}

void PopulateMonitors(QComboBox *combo)
{
	combo->clear();
	const auto screens = QGuiApplication::screens();
	for (int i = 0; i < screens.size(); ++i) {
		const QScreen *screen = screens.at(i);
		QString label = QString("Pantalla %1").arg(i + 1);
		if (screen) {
			const QRect geo = screen->geometry();
			label = QString("%1 - %2x%3 @ %4,%5")
					.arg(screen->name().isEmpty() ? label : screen->name())
					.arg(geo.width())
					.arg(geo.height())
					.arg(geo.x())
					.arg(geo.y());
		}
		combo->addItem(label, i);
	}
	if (combo->count() == 0)
		combo->addItem("Cap pantalla detectada", -1);
}

void FrontendEventCallback(enum obs_frontend_event event, void *)
{
	TimeTracker::Instance().HandleFrontendEvent(static_cast<int>(event));
}

void OpenMonitorWindow(void *)
{
	OpenPluginMonitor(nullptr);
}

void OpenSettingsWindow(void *)
{
	ShowSettingsDialog(MainWindow());
}

void OpenPluginMonitor(void *)
{
	if (!g_dialog) {
		QWidget *mainWindow = static_cast<QWidget *>(obs_frontend_get_main_window());
		g_dialog = new ProgramMonitorDialog(mainWindow);
	}
	const int screenIndex = g_monitorCombo ? g_monitorCombo->currentData().toInt() : -1;
	g_dialog->ShowOnScreen(screenIndex, true);
}

void CloseMonitorWindow(void *)
{
	if (g_dialog)
		g_dialog->close();
}

void OpenQuickMessageDialog(void *)
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
	g_messageDialog->show();
	g_messageDialog->raise();
	g_messageDialog->activateWindow();
}

void OpenPreferencesDialog(void *)
{
	ShowSettingsDialog(MainWindow());
}

QWidget *CreateMonitorDock(QWidget *parent)
{
	auto *dock = new QWidget(parent);
	dock->setObjectName("rec-time-monitor-dock");

	auto *layout = new QVBoxLayout(dock);
	layout->setContentsMargins(12, 12, 12, 12);
	layout->setSpacing(8);

	auto *title = new QLabel("Control ràpid del REC Time Monitor", dock);
	title->setWordWrap(true);
	layout->addWidget(title);

	auto *monitorLabel = new QLabel("Pantalla de la finestra del plugin", dock);
	layout->addWidget(monitorLabel);

	g_monitorCombo = new QComboBox(dock);
	PopulateMonitors(g_monitorCombo);
	layout->addWidget(g_monitorCombo);

	// Re-scan whenever a monitor is plugged/unplugged so the list doesn't go
	// stale for the lifetime of the OBS session.
	QObject::connect(qGuiApp, &QGuiApplication::screenAdded, g_monitorCombo,
			 [](QScreen *) { PopulateMonitors(g_monitorCombo); });
	QObject::connect(qGuiApp, &QGuiApplication::screenRemoved, g_monitorCombo,
			 [](QScreen *) { PopulateMonitors(g_monitorCombo); });

	auto *projectorButton = new QPushButton("Obrir el plugin a pantalla sencera", dock);
	projectorButton->setMinimumHeight(36);
	QObject::connect(projectorButton, &QPushButton::clicked, dock, []() { OpenPluginMonitor(nullptr); });
	layout->addWidget(projectorButton);

	auto *closeButton = new QPushButton("Tanca el monitor", dock);
	closeButton->setMinimumHeight(36);
	QObject::connect(closeButton, &QPushButton::clicked, dock, []() { CloseMonitorWindow(nullptr); });
	layout->addWidget(closeButton);

	auto *messageButton = new QPushButton("Obrir editor de missatge", dock);
	messageButton->setMinimumHeight(36);
	QObject::connect(messageButton, &QPushButton::clicked, dock, []() { OpenQuickMessageDialog(nullptr); });
	layout->addWidget(messageButton);

	auto *preferencesButton = new QPushButton("Preferències", dock);
	preferencesButton->setMinimumHeight(36);
	QObject::connect(preferencesButton, &QPushButton::clicked, dock, []() { OpenPreferencesDialog(nullptr); });
	layout->addWidget(preferencesButton);

	layout->addStretch();

	return dock;
}

} // namespace

bool obs_module_load(void)
{
	TimeTracker::Instance().SyncCurrentState();
	obs_frontend_add_event_callback(FrontendEventCallback, nullptr);
	obs_frontend_add_tools_menu_item("Monitor de Programa + Cronòmetre", OpenMonitorWindow, nullptr);
	obs_frontend_add_tools_menu_item("Configuració del Monitor de Programa", OpenSettingsWindow, nullptr);

	if (!g_dock) {
		g_dock = CreateMonitorDock(MainWindow());
		obs_frontend_add_dock_by_id("rec-time-monitor-dock", "REC Time Monitor", g_dock);
	}

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
