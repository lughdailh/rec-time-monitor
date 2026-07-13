#pragma once

#include <QWidget>

class QComboBox;
class QSpinBox;
class QVBoxLayout;

// Non-modal settings panel for the REC Time Monitor plugin. Every control
// applies immediately to Settings (see settings.hpp); there is no separate
// Apply/OK step.
class SettingsDialog : public QWidget {
	Q_OBJECT

public:
	explicit SettingsDialog(QWidget *parent = nullptr);

private:
	void RebuildAlertRows();

	QVBoxLayout *alertsLayout_ = nullptr;
	QComboBox *cornerCombo_ = nullptr;
	QSpinBox *scaleSpin_ = nullptr;
};

// Shows the single shared SettingsDialog instance, creating it on first use
// and just raising it on subsequent calls. Both the Tools menu item and
// ProgramMonitorDialog's right-click menu go through this so there is only
// ever one settings window regardless of which one opened it.
void ShowSettingsDialog(QWidget *parent);
