#pragma once

#include <QMutex>
#include <QString>
#include <QStringList>
#include <QVector>

#include <atomic>

// One configurable time checkpoint: once the active recording/streaming
// timer reaches `seconds`, the pulsing border switches to `colorHex`.
struct AlertEntry {
	int seconds = 60;
	QString colorHex = "#ff3b30";
	int durationSeconds = 5;
	QString message;
};

// Persisted plugin settings, shared between SettingsDialog (Qt UI thread)
// and OBSProgramDisplay::Draw (OBS video thread). Corner/ScalePercent are
// read directly from the video thread every frame, hence plain atomics; the
// alert list is only ever touched from the Qt UI thread (SettingsDialog and
// ProgramMonitorDialog's timer both live there), guarded by a mutex mainly
// for defensiveness.
class Settings {
public:
	enum class BadgeCorner { TopLeft, TopRight, BottomLeft, BottomRight, TopCenter, BottomCenter };

	static Settings &Instance();

	BadgeCorner Corner() const { return static_cast<BadgeCorner>(corner_.load()); }
	void SetCorner(BadgeCorner corner);

	// Percent, 100-300 (100 = today's normal size, 300 = largest).
	int ScalePercent() const { return scalePercent_.load(); }
	void SetScalePercent(int percent);

	QString MonitorTarget() const { return monitorTarget_; }
	void SetMonitorTarget(const QString &target);
	bool MonitorEnabled() const { return monitorEnabled_; }
	void SetMonitorEnabled(bool enabled);
	bool MonitorLocked() const { return monitorLocked_; }
	void SetMonitorLocked(bool locked);
	QStringList MessagePresets() const { return messagePresets_; }
	void SetMessagePresets(const QStringList &presets);

	QVector<AlertEntry> Alerts() const;
	int AddAlert(int seconds, const QString &colorHex);
	void RemoveAlert(int index);
	void SetAlertSeconds(int index, int seconds);
	void SetAlertColor(int index, const QString &colorHex);
	void SetAlertDuration(int index, int seconds);
	void SetAlertMessage(int index, const QString &message);

private:
	Settings();
	void Load();
	void Save();

	std::atomic<int> corner_{static_cast<int>(BadgeCorner::TopRight)};
	std::atomic<int> scalePercent_{200};
	QString monitorTarget_;
	bool monitorEnabled_ = false;
	bool monitorLocked_ = false;
	QStringList messagePresets_;

	mutable QMutex alertsMutex_;
	QVector<AlertEntry> alerts_;
};
