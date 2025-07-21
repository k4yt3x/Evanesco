#include "prefdialog.h"
#include "ui_prefdialog.h"

#include <QApplication>
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
    ui->hideTaskbarIconCheckBox->setChecked(settings->hideTaskbarIcon());
    ui->randomizeDllFileNameCheckBox->setChecked(settings->randomizeDllFileName());

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
    settings->setHideTaskbarIcon(ui->hideTaskbarIconCheckBox->isChecked());
    settings->setRandomizeDllFileName(ui->randomizeDllFileNameCheckBox->isChecked());

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
