#include "settings.hpp"

#include <obs-module.h>
#include <util/bmem.h>

#include <QDir>
#include <QMutexLocker>
#include <QSettings>

namespace {

QString ConfigFilePath()
{
	char *dir = obs_module_config_path(nullptr);
	QString path = QString::fromUtf8(dir ? dir : "");
	bfree(dir);

	if (path.isEmpty())
		return QString();

	QDir().mkpath(path);
	return path + "/settings.ini";
}

} // namespace

Settings &Settings::Instance()
{
	static Settings instance;
	return instance;
}

Settings::Settings()
{
	Load();
}

void Settings::Load()
{
	const QString path = ConfigFilePath();
	if (path.isEmpty())
		return;

	QSettings settings(path, QSettings::IniFormat);
	corner_ = settings.value("corner", static_cast<int>(BadgeCorner::TopRight)).toInt();
	scalePercent_ = settings.value("scalePercent", 200).toInt();

	QVector<AlertEntry> alerts;
	const int count = settings.beginReadArray("alerts");
	for (int i = 0; i < count; i++) {
		settings.setArrayIndex(i);
		AlertEntry entry;
		entry.seconds = settings.value("seconds", 60).toInt();
		entry.colorHex = settings.value("colorHex", "#ff3b30").toString();
		alerts.append(entry);
	}
	settings.endArray();

	// One-time migration from the earlier single-alarm-in-minutes setting.
	if (alerts.isEmpty() && settings.contains("alarmMinutes")) {
		const int legacyMinutes = settings.value("alarmMinutes", 0).toInt();
		if (legacyMinutes > 0)
			alerts.append(AlertEntry{legacyMinutes * 60, "#ff3b30"});
	}

	QMutexLocker locker(&alertsMutex_);
	alerts_ = alerts;
}

void Settings::Save()
{
	const QString path = ConfigFilePath();
	if (path.isEmpty())
		return;

	QSettings settings(path, QSettings::IniFormat);
	settings.remove("alarmMinutes"); // superseded by the "alerts" array
	settings.setValue("corner", corner_.load());
	settings.setValue("scalePercent", scalePercent_.load());

	QVector<AlertEntry> alerts;
	{
		QMutexLocker locker(&alertsMutex_);
		alerts = alerts_;
	}

	settings.beginWriteArray("alerts");
	for (int i = 0; i < alerts.size(); i++) {
		settings.setArrayIndex(i);
		settings.setValue("seconds", alerts[i].seconds);
		settings.setValue("colorHex", alerts[i].colorHex);
	}
	settings.endArray();
}

void Settings::SetCorner(BadgeCorner corner)
{
	corner_ = static_cast<int>(corner);
	Save();
}

void Settings::SetScalePercent(int percent)
{
	scalePercent_ = percent;
	Save();
}

QVector<AlertEntry> Settings::Alerts() const
{
	QMutexLocker locker(&alertsMutex_);
	return alerts_;
}

int Settings::AddAlert(int seconds, const QString &colorHex)
{
	int index;
	{
		QMutexLocker locker(&alertsMutex_);
		alerts_.append(AlertEntry{seconds, colorHex});
		index = alerts_.size() - 1;
	}
	Save();
	return index;
}

void Settings::RemoveAlert(int index)
{
	{
		QMutexLocker locker(&alertsMutex_);
		if (index < 0 || index >= alerts_.size())
			return;
		alerts_.remove(index);
	}
	Save();
}

void Settings::SetAlertSeconds(int index, int seconds)
{
	{
		QMutexLocker locker(&alertsMutex_);
		if (index < 0 || index >= alerts_.size())
			return;
		alerts_[index].seconds = seconds;
	}
	Save();
}

void Settings::SetAlertColor(int index, const QString &colorHex)
{
	{
		QMutexLocker locker(&alertsMutex_);
		if (index < 0 || index >= alerts_.size())
			return;
		alerts_[index].colorHex = colorHex;
	}
	Save();
}
