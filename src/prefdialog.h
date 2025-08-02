#pragma once

#include <windows.h>

#include <QDialog>
#include <QListWidgetItem>
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

    // Autohide tab slots
    void onAutohideAddClicked();
    void onAutohideRemoveClicked();
    void onAutohideSelectFileClicked();
    void onAutohideItemChanged(QListWidgetItem* item);

   private:
    Ui::PrefDialog* ui;
    Settings* settings;

    void loadSettings();
    void saveSettings();
    void resetToDefaults();

    // Autohide helper methods
    void updateAutohideList();
    void addAutohideEntry(const QString& entry);
    QStringList getAllAutohideEntries() const;
};
