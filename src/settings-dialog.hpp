#pragma once

#include <QWidget>

#include <functional>

class QComboBox;
class QSpinBox;
class QVBoxLayout;

// Non-modal settings panel for the REC Time Monitor plugin. Every control
// applies immediately to Settings (see settings.hpp); there is no separate
// Apply/OK step. Appearance and time marks live on separate tabs.
class SettingsDialog : public QWidget {
	Q_OBJECT

public:
	using MonitorTargetCallback = std::function<void(const QString &)>;

	explicit SettingsDialog(MonitorTargetCallback monitorTargetCallback, QWidget *parent = nullptr);

private:
	void PopulateMonitorTargets();
	void RebuildAlertRows();

	MonitorTargetCallback monitorTargetCallback_;
	QVBoxLayout *alertsLayout_ = nullptr;
	QComboBox *monitorTargetCombo_ = nullptr;
	QComboBox *cornerCombo_ = nullptr;
	QSpinBox *scaleSpin_ = nullptr;
};

// Shows the single shared SettingsDialog instance, creating it on first use
// and just raising it on subsequent calls.
void ShowSettingsDialog(QWidget *parent, SettingsDialog::MonitorTargetCallback monitorTargetCallback);
void PrepareSettingsDialog(QWidget *parent, SettingsDialog::MonitorTargetCallback monitorTargetCallback);
