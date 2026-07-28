#include "quick-message-dialog.hpp"
#include "settings.hpp"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <obs-module.h>

QuickMessageDialog::QuickMessageDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(obs_module_text("Message.Title"));
	setModal(false);
	setMinimumWidth(620);
	setMinimumHeight(400);
	resize(650, 440);

	auto *rootLayout = new QHBoxLayout(this);
	rootLayout->setContentsMargins(12, 12, 12, 12);
	rootLayout->setSpacing(12);

	auto *editorPanel = new QWidget(this);
	auto *editorLayout = new QVBoxLayout(editorPanel);
	editorLayout->setContentsMargins(0, 0, 0, 0);
	editorLayout->setSpacing(8);

	auto *info = new QLabel(obs_module_text("Message.Help"), editorPanel);
	info->setWordWrap(true);
	editorLayout->addWidget(info);

	messageEdit_ = new QPlainTextEdit(editorPanel);
	messageEdit_->setPlaceholderText(obs_module_text("Message.Placeholder"));
	messageEdit_->setMinimumHeight(210);
	editorLayout->addWidget(messageEdit_, 1);

	auto *form = new QFormLayout();
	durationSpin_ = new QSpinBox(editorPanel);
	durationSpin_->setRange(1, 600);
	durationSpin_->setSuffix(" s");
	durationSpin_->setValue(5);
	form->addRow(obs_module_text("Message.Duration"), durationSpin_);
	editorLayout->addLayout(form);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, editorPanel);
	buttons->button(QDialogButtonBox::Ok)->setText(obs_module_text("Message.Send"));
	connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
		emit SendRequested(messageEdit_->toPlainText(), durationSpin_->value() * 1000);
		accept();
	});
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	editorLayout->addWidget(buttons);
	rootLayout->addWidget(editorPanel, 1);

	auto *presetsGroup = new QGroupBox(obs_module_text("Message.Presets"), this);
	presetsGroup->setFixedWidth(220);
	auto *presetsLayout = new QVBoxLayout(presetsGroup);
	presetsLayout->setContentsMargins(10, 14, 10, 10);
	presetsLayout->setSpacing(8);

	auto *presetsList = new QListWidget(presetsGroup);
	QStringList presets = Settings::Instance().MessagePresets();
	if (presets.size() != 6) {
		presets.clear();
		for (const char *key : {"Message.Preset1", "Message.Preset2", "Message.Preset3", "Message.Preset4",
					"Message.Preset5", "Message.Preset6"})
			presets.append(obs_module_text(key));
		Settings::Instance().SetMessagePresets(presets);
	}
	presetsList->addItems(presets);
	presetsList->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
	for (int index = 0; index < presetsList->count(); ++index) {
		QListWidgetItem *item = presetsList->item(index);
		item->setFlags(item->flags() | Qt::ItemIsEditable);
		item->setData(Qt::UserRole, item->text());
	}
	presetsList->setWordWrap(true);
	presetsLayout->addWidget(presetsList, 1);

	auto *sendPresetButton = new QPushButton(obs_module_text("Message.SendPreset"), presetsGroup);
	sendPresetButton->setEnabled(false);
	presetsLayout->addWidget(sendPresetButton);

	auto sendPreset = [this, presetsList]() {
		QListWidgetItem *item = presetsList->currentItem();
		if (!item)
			return;
		emit SendRequested(item->text(), durationSpin_->value() * 1000);
	};
	connect(presetsList, &QListWidget::currentItemChanged, sendPresetButton,
		[sendPresetButton](QListWidgetItem *current) { sendPresetButton->setEnabled(current != nullptr); });
	connect(sendPresetButton, &QPushButton::clicked, this, sendPreset);
	connect(presetsList, &QListWidget::itemChanged, this, [presetsList](QListWidgetItem *item) {
		const QString edited = item->text().trimmed();
		if (edited.isEmpty()) {
			const QSignalBlocker blocker(presetsList);
			item->setText(item->data(Qt::UserRole).toString());
			return;
		}

		{
			const QSignalBlocker blocker(presetsList);
			if (edited != item->text())
				item->setText(edited);
			item->setData(Qt::UserRole, edited);
		}

		QStringList updated;
		for (int index = 0; index < presetsList->count(); ++index)
			updated.append(presetsList->item(index)->text());
		Settings::Instance().SetMessagePresets(updated);
	});

	rootLayout->addWidget(presetsGroup);

	messageEdit_->setFocus();
}
