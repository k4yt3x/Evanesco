#include "aboutdialog.h"
#include "ui_aboutdialog.h"

AboutDialog::AboutDialog(QWidget* parent, QString version) : QDialog(parent), ui(new Ui::AboutDialog) {
    ui->setupUi(this);
    ui->titleLabel->setText("Evanesco " + version);
}

AboutDialog::~AboutDialog() {
    delete ui;
}
