#pragma once

#include <QImage>
#include <QString>

// Renders a quick message banner for the projector overlay.
QImage RenderMessageImage(const QString &message, double scale);
