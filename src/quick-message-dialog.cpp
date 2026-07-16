#include "quick-message-dialog.hpp"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

QuickMessageDialog::QuickMessageDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle("Missatge ràpid");
	setModal(false);
	setMinimumWidth(820);
	setMinimumHeight(420);
	resize(920, 460);

	auto *rootLayout = new QVBoxLayout(this);

	auto *info = new QLabel("Escriu el missatge i la durada. Es mostrarà sobre la finestra del plugin.", this);
	info->setWordWrap(true);
	rootLayout->addWidget(info);

	messageEdit_ = new QPlainTextEdit(this);
	messageEdit_->setPlaceholderText("Escriu aquí el text que vols mostrar...");
	messageEdit_->setMinimumHeight(240);
	rootLayout->addWidget(messageEdit_);

	auto *form = new QFormLayout();
	durationSpin_ = new QSpinBox(this);
	durationSpin_->setRange(1, 600);
	durationSpin_->setSuffix(" s");
	durationSpin_->setValue(5);
	form->addRow("Durada del missatge:", durationSpin_);
	rootLayout->addLayout(form);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this);
	buttons->button(QDialogButtonBox::Ok)->setText("Enviar");
	connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
		emit SendRequested(messageEdit_->toPlainText(), durationSpin_->value() * 1000);
		accept();
	});
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	rootLayout->addWidget(buttons);

	messageEdit_->setFocus();
}
