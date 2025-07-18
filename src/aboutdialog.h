#pragma once

#include <windows.h>

#include <QDialog>
#include <QShowEvent>

namespace Ui {
class AboutDialog;
}

class AboutDialog : public QDialog {
    Q_OBJECT

   public:
    explicit AboutDialog(QWidget* parent, QString version);
    ~AboutDialog();

   protected:
    void showEvent(QShowEvent* event) override;

   private:
    Ui::AboutDialog* ui;
};
