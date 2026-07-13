#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <QPointer>
#include <QWidget>

#include "program-monitor-dialog.hpp"
#include "settings-dialog.hpp"
#include "time-tracker.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {

QPointer<ProgramMonitorDialog> g_dialog;

void FrontendEventCallback(enum obs_frontend_event event, void *)
{
	TimeTracker::Instance().HandleFrontendEvent(static_cast<int>(event));
}

void OpenMonitorWindow(void *)
{
	if (!g_dialog) {
		QWidget *mainWindow = static_cast<QWidget *>(obs_frontend_get_main_window());
		g_dialog = new ProgramMonitorDialog(mainWindow);
	}
	g_dialog->show();
	g_dialog->raise();
	g_dialog->activateWindow();
}

void OpenSettingsWindow(void *)
{
	ShowSettingsDialog(static_cast<QWidget *>(obs_frontend_get_main_window()));
}

} // namespace

bool obs_module_load(void)
{
	TimeTracker::Instance().SyncCurrentState();
	obs_frontend_add_event_callback(FrontendEventCallback, nullptr);
	obs_frontend_add_tools_menu_item("Monitor de Programa + Cronòmetre", OpenMonitorWindow, nullptr);
	obs_frontend_add_tools_menu_item("Configuració del Monitor de Programa", OpenSettingsWindow, nullptr);

	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(FrontendEventCallback, nullptr);
	obs_log(LOG_INFO, "plugin unloaded");
}
