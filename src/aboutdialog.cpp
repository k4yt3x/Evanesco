#include "aboutdialog.h"
#include "ui_aboutdialog.h"

#include "settings.h"
#include "titleutils.h"

AboutDialog::AboutDialog(QWidget* parent, QString version) : QDialog(parent), ui(new Ui::AboutDialog) {
    ui->setupUi(this);
    ui->titleLabel->setText("Evanesco " + version);
}

AboutDialog::~AboutDialog() {
    delete ui;
}

void AboutDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);

    Settings* settings = Settings::instance();

    // Apply screen capture hiding when dialog is shown
    if (settings->hideFromScreenCapture()) {
        HWND hwnd = reinterpret_cast<HWND>(this->winId());
        SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
    }

    // Apply randomized title when dialog is shown
    if (settings->randomizeWindowTitles()) {
        this->setWindowTitle(TitleUtils::generateRandomTitle());
    }
}
