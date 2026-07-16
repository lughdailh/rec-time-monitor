#include "message-image.hpp"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>

#include <algorithm>

namespace {

constexpr int kBasePadding = 22;
constexpr int kBasePixelSize = 42;
constexpr int kBaseCornerRadius = 16;

QFont MessageFont(double scale)
{
	QFont f;
	f.setBold(true);
	f.setPixelSize(std::max(1, static_cast<int>(kBasePixelSize * scale)));
	return f;
}

} // namespace

QImage RenderMessageImage(const QString &message, double scale)
{
	scale = std::clamp(scale, 1.0, 3.0);
	const QString text = message.trimmed().isEmpty() ? QStringLiteral("MISSATGE") : message.trimmed();

	const int padding = static_cast<int>(kBasePadding * scale);
	const QFont font = MessageFont(scale);
	const QFontMetrics metrics(font);

	const int textWidth = metrics.horizontalAdvance(text);
	const int textHeight = metrics.height();
	const int width = textWidth + padding * 2;
	const int height = textHeight + padding * 2;

	QImage image(width, height, QImage::Format_ARGB32);
	image.fill(Qt::transparent);

	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(Qt::NoPen);
	painter.setBrush(QColor(0, 0, 0, 190));
	painter.drawRoundedRect(QRectF(0, 0, width, height), kBaseCornerRadius * scale, kBaseCornerRadius * scale);

	painter.setPen(Qt::white);
	painter.setFont(font);
	painter.drawText(QRect(0, 0, width, height), Qt::AlignCenter, text);
	painter.end();

	return image;
}
