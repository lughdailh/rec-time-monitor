#pragma once

#include <QImage>
#include <QString>

// Renders the REC/LIVE badge (dot + label + time, dark rounded pill
// background) into a standalone ARGB32 image with a transparent background.
// This used to be a Qt widget overlaid on top of OBSProgramDisplay, but
// stacking a Qt window (whether a native child widget or a separate
// top-level window) reliably above/in-sync with a native embedded OBS render
// surface turned out to be fighting Qt/Cocoa's window compositing the whole
// way: partial repaints left ghosted frames behind, and promoting the badge
// to its own top-level window made it float independently instead of
// tracking the program window.
//
// Producing a plain image here sidesteps all of that: OBSProgramDisplay
// uploads it as a GPU texture and draws it as the very last thing in the
// same draw callback as the video itself (see program-display.mm), so it is
// always correctly on top, in perfect sync, with no separate window to lose
// track of.
// scale 1.0 = default size; the caller (SettingsDialog) offers roughly
// 0.5-2.0.
QImage RenderBadgeImage(const QString &colorHex, const QString &badgeText, const QString &timeText, double scale);
