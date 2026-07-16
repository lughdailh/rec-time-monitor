#pragma once

#include <QDialog>

class QPlainTextEdit;
class QSpinBox;

class QuickMessageDialog : public QDialog {
	Q_OBJECT

public:
	explicit QuickMessageDialog(QWidget *parent = nullptr);

signals:
	void SendRequested(const QString &text, int durationMs);

private:
	QPlainTextEdit *messageEdit_ = nullptr;
	QSpinBox *durationSpin_ = nullptr;
};
