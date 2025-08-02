#include "prefdialog.h"
#include "ui_prefdialog.h"

#include <QApplication>
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>

#include "randutils.h"
#include "settings.h"

PrefDialog::PrefDialog(QWidget* parent) : QDialog(parent), ui(new Ui::PrefDialog) {
    ui->setupUi(this);

    // Get settings instance
    settings = Settings::instance();

    // Connect button box signals
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &PrefDialog::onOkClicked);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &PrefDialog::onCancelClicked);
    connect(ui->buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &PrefDialog::onApplyClicked);

    // Connect reset button
    connect(ui->resetPushButton, &QPushButton::clicked, this, &PrefDialog::resetToDefaults);

    // Connect autohide tab signals
    connect(ui->autohideAddPushButton, &QPushButton::clicked, this, &PrefDialog::onAutohideAddClicked);
    connect(ui->autohideRemovePushButton, &QPushButton::clicked, this, &PrefDialog::onAutohideRemoveClicked);
    connect(ui->autohideSelectFilePushButton, &QPushButton::clicked, this, &PrefDialog::onAutohideSelectFileClicked);
    connect(ui->autohideListWidget, &QListWidget::itemChanged, this, &PrefDialog::onAutohideItemChanged);
    connect(ui->autohideListWidget, &QListWidget::currentRowChanged, this, [this](int currentRow) {
        ui->autohideRemovePushButton->setEnabled(currentRow >= 0);
    });
    connect(ui->autohideFileNameLineEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        ui->autohideAddPushButton->setEnabled(!text.trimmed().isEmpty());
    });

    // Load current settings into UI
    loadSettings();

    // Apply screen capture hiding to this dialog
    if (settings->hideFromScreenCapture()) {
        HWND hwnd = reinterpret_cast<HWND>(this->winId());
        SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
    }
}

PrefDialog::~PrefDialog() {
    delete ui;
}

void PrefDialog::loadSettings() {
    // Load settings from Settings singleton
    ui->autoRefreshCheckBox->setChecked(settings->autoRefresh());
    ui->autoRefreshIntervalDoubleSpinBox->setValue(settings->refreshInterval());
    ui->autoRefreshIntervalDoubleSpinBox->setEnabled(settings->autoRefresh());
    ui->hideFromScreenCaptureCheckBox->setChecked(settings->hideFromScreenCapture());
    ui->randomizeWindowTitlesCheckBox->setChecked(settings->randomizeWindowTitles());
    ui->randomizeTrayIconCheckBox->setChecked(settings->randomizeTrayIcon());
    ui->minimizeToTrayCheckBox->setChecked(settings->minimizeToTray());
    ui->maxWindowCreationWaitMsSpinBox->setValue(settings->maxWindowCreationWaitMs());
    ui->hideTaskbarIconCheckBox->setChecked(settings->hideTaskbarIcon());
    ui->randomizeDllFileNameCheckBox->setChecked(settings->randomizeDllFileName());
    ui->hideTargetTaskbarIconsCheckBox->setChecked(settings->hideTargetTaskbarIcons());
    ui->enableAutohideCheckBox->setChecked(settings->autohideEnabled());
    ui->autohideNotifyCheckBox->setChecked(settings->autohideNotify());
    ui->autohideAddPushButton->setEnabled(!ui->autohideFileNameLineEdit->text().trimmed().isEmpty());
    updateAutohideList();

    // Connect the auto refresh checkbox to enable/disable interval spinbox
    connect(
        ui->autoRefreshCheckBox, &QCheckBox::toggled, ui->autoRefreshIntervalDoubleSpinBox, &QDoubleSpinBox::setEnabled
    );
}

void PrefDialog::saveSettings() {
    // Save current UI values to settings
    settings->setAutoRefresh(ui->autoRefreshCheckBox->isChecked());
    settings->setRefreshInterval(ui->autoRefreshIntervalDoubleSpinBox->value());
    settings->setHideFromScreenCapture(ui->hideFromScreenCaptureCheckBox->isChecked());
    settings->setRandomizeWindowTitles(ui->randomizeWindowTitlesCheckBox->isChecked());
    settings->setRandomizeTrayIcon(ui->randomizeTrayIconCheckBox->isChecked());
    settings->setMinimizeToTray(ui->minimizeToTrayCheckBox->isChecked());
    settings->setMaxWindowCreationWaitMs(ui->maxWindowCreationWaitMsSpinBox->value());
    settings->setHideTaskbarIcon(ui->hideTaskbarIconCheckBox->isChecked());
    settings->setRandomizeDllFileName(ui->randomizeDllFileNameCheckBox->isChecked());
    settings->setHideTargetTaskbarIcons(ui->hideTargetTaskbarIconsCheckBox->isChecked());
    settings->setAutohideEnabled(ui->enableAutohideCheckBox->isChecked());
    settings->setAutohideNotify(ui->autohideNotifyCheckBox->isChecked());
    settings->setAutohideList(getAllAutohideEntries());

    // Force write to disk
    settings->sync();
}

void PrefDialog::resetToDefaults() {
    // Reset to default values using Settings
    settings->resetToDefaults();

    // Update UI to reflect defaults
    loadSettings();
}

void PrefDialog::onOkClicked() {
    saveSettings();
    accept();
}

void PrefDialog::onApplyClicked() {
    saveSettings();
    // Don't close dialog, just apply changes
}

void PrefDialog::onCancelClicked() {
    // Reload settings to discard any changes
    loadSettings();
    reject();
}

void PrefDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);

    Settings* settings = Settings::instance();

    // Apply screen capture hiding when dialog is shown
    if (settings->hideFromScreenCapture()) {
        HWND hwnd = reinterpret_cast<HWND>(this->winId());
        SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
    }

    // Apply randomized title when dialog is shown
    if (settings->randomizeWindowTitles()) {
        this->setWindowTitle(RandUtils::generateRandomTitle());
    }
}

void PrefDialog::updateAutohideList() {
    ui->autohideListWidget->clear();

    QStringList entries = settings->autohideList();
    for (const QString& entry : entries) {
        addAutohideEntry(entry);
    }

    // Update remove button state
    ui->autohideRemovePushButton->setEnabled(ui->autohideListWidget->currentRow() >= 0);
}

void PrefDialog::addAutohideEntry(const QString& entry) {
    QListWidgetItem* item = new QListWidgetItem(entry);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    ui->autohideListWidget->addItem(item);
}

QStringList PrefDialog::getAllAutohideEntries() const {
    QStringList entries;
    for (int i = 0; i < ui->autohideListWidget->count(); ++i) {
        QListWidgetItem* item = ui->autohideListWidget->item(i);
        if (item) {
            QString text = item->text().trimmed();
            if (!text.isEmpty()) {
                entries.append(text);
            }
        }
    }
    return entries;
}

void PrefDialog::onAutohideAddClicked() {
    QString entry = ui->autohideFileNameLineEdit->text().trimmed();

    if (entry.isEmpty()) {
        QMessageBox::warning(this, "Empty Entry", "Please enter a filename or path, or use the Select File button.");
        return;
    }

    // Check if entry already exists
    QStringList allEntries = settings->autohideList();

    if (allEntries.contains(entry, Qt::CaseInsensitive)) {
        QMessageBox::information(
            this, "Duplicate Entry", QString("The entry '%1' already exists in the list.").arg(entry)
        );
        return;
    }

    // Add to settings
    allEntries.append(entry);
    settings->setAutohideList(allEntries);

    // Update the display
    updateAutohideList();

    // Clear the line edit
    ui->autohideFileNameLineEdit->clear();

    // Select the new item
    for (int i = 0; i < ui->autohideListWidget->count(); ++i) {
        if (ui->autohideListWidget->item(i)->text() == entry) {
            ui->autohideListWidget->setCurrentRow(i);
            break;
        }
    }
}

void PrefDialog::onAutohideSelectFileClicked() {
    QString filePath =
        QFileDialog::getOpenFileName(this, "Select Executable File", "", "Executable Files (*.exe);;All Files (*.*)");

    if (!filePath.isEmpty()) {
        ui->autohideFileNameLineEdit->setText(filePath);
    }
}

void PrefDialog::onAutohideRemoveClicked() {
    int currentRow = ui->autohideListWidget->currentRow();
    if (currentRow < 0) {
        return;
    }

    QListWidgetItem* item = ui->autohideListWidget->item(currentRow);
    if (!item) {
        return;
    }

    QString entryToRemove = item->text();

    // Remove from settings
    QStringList allEntries = settings->autohideList();
    allEntries.removeAll(entryToRemove);
    settings->setAutohideList(allEntries);

    // Update the display
    updateAutohideList();
}

void PrefDialog::onAutohideItemChanged(QListWidgetItem* item) {
    if (!item) {
        return;
    }

    QString newText = item->text().trimmed();

    // Validate the new text
    if (newText.isEmpty()) {
        QMessageBox::warning(this, "Invalid Entry", "Entry cannot be empty.");
        return;
    }

    // Check for duplicates
    QStringList currentEntries = getAllAutohideEntries();
    int duplicateCount = 0;
    for (const QString& entry : currentEntries) {
        if (entry.compare(newText, Qt::CaseInsensitive) == 0) {
            duplicateCount++;
        }
    }

    if (duplicateCount > 1) {
        QMessageBox::warning(
            this, "Duplicate Entry", QString("The entry '%1' already exists in the list.").arg(newText)
        );
        updateAutohideList();
        return;
    }

    // Update settings with current list
    settings->setAutohideList(currentEntries);
}
