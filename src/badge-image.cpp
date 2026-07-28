#include "badge-image.hpp"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>

#include <algorithm>
#include <obs-module.h>

namespace {

constexpr int kBasePadding = 20;
constexpr int kBaseDotSize = 16;
constexpr int kBaseSpacing = 14;
constexpr int kBaseLabelPixelSize = 18;
constexpr int kBaseTimePixelSize = 44;
constexpr int kBaseCornerRadius = 14;

QFont LabelFont(double scale)
{
	QFont f;
	f.setBold(true);
	f.setPixelSize(std::max(1, static_cast<int>(kBaseLabelPixelSize * scale)));
	f.setLetterSpacing(QFont::AbsoluteSpacing, 2 * scale);
	return f;
}

QFont TimeFont(double scale)
{
	QFont f;
	f.setBold(true);
	f.setPixelSize(std::max(1, static_cast<int>(kBaseTimePixelSize * scale)));
	f.setStyleHint(QFont::Monospace);
	return f;
}

} // namespace

QImage RenderBadgeImage(const QString &colorHex, const QString &badgeText, const QString &timeText, double scale)
{
	scale = std::clamp(scale, 1.0, 3.0);

	const int padding = static_cast<int>(kBasePadding * scale);
	const int dotSize = static_cast<int>(kBaseDotSize * scale);
	const int spacing = static_cast<int>(kBaseSpacing * scale);

	const QFontMetrics labelMetrics(LabelFont(scale));
	const QFontMetrics timeMetrics(TimeFont(scale));

	// Reserve enough room for every translated state so the badge never
	// changes size (or clips text) as recording state changes.
	const int labelWidth = std::max({labelMetrics.horizontalAdvance(obs_module_text("Status.Standby")),
					 labelMetrics.horizontalAdvance(obs_module_text("Status.Paused")),
					 labelMetrics.horizontalAdvance(obs_module_text("Status.Recording")),
					 labelMetrics.horizontalAdvance(obs_module_text("Status.Streaming"))});
	const int timeWidth = timeMetrics.horizontalAdvance("00:00:00");

	const int width = padding + dotSize + spacing + labelWidth + spacing + timeWidth + padding;
	const int contentHeight = std::max(timeMetrics.height(), std::max(labelMetrics.height(), dotSize));
	const int height = contentHeight + 2 * (padding - static_cast<int>(6 * scale));

	QImage image(width, height, QImage::Format_ARGB32);
	image.fill(Qt::transparent);

	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);

	painter.setPen(Qt::NoPen);
	painter.setBrush(QColor(0, 0, 0, 160));
	painter.drawRoundedRect(QRectF(0, 0, width, height), kBaseCornerRadius * scale, kBaseCornerRadius * scale);

	int x = padding;

	painter.setBrush(QColor(colorHex));
	painter.drawEllipse(QPointF(x + dotSize / 2.0, height / 2.0), dotSize / 2.0, dotSize / 2.0);
	x += dotSize + spacing;

	painter.setPen(Qt::white);
	painter.setFont(LabelFont(scale));
	painter.drawText(QRect(x, 0, labelWidth, height), Qt::AlignVCenter | Qt::AlignLeft, badgeText);
	x += labelWidth + spacing;

	painter.setFont(TimeFont(scale));
	painter.drawText(QRect(x, 0, timeWidth, height), Qt::AlignVCenter | Qt::AlignLeft, timeText);

	painter.end();

	return image;
}
