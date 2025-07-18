#pragma once

#include <windows.h>

#include <QDialog>
#include <QShowEvent>

namespace Ui {
class PrefDialog;
}

class Settings;

class PrefDialog : public QDialog {
    Q_OBJECT

   public:
    explicit PrefDialog(QWidget* parent = nullptr);
    ~PrefDialog();

   protected:
    void showEvent(QShowEvent* event) override;

   private slots:
    void onOkClicked();
    void onApplyClicked();
    void onCancelClicked();

   private:
    Ui::PrefDialog* ui;
    Settings* settings;

    void loadSettings();
    void saveSettings();
    void resetToDefaults();
};
