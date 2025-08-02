#pragma once

// clang-format off
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
// clang-format on

#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QDoubleSpinBox>
#include <QFileIconProvider>
#include <QHeaderView>
#include <QIcon>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QMainWindow>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QSet>
#include <QShowEvent>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>

#include "autohider.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

struct ProcessInfo {
    DWORD pid;
    QString processName;
    QString architecture;
    QIcon icon;
    QString executablePath;
    HWND windowHandle;
};

struct WindowInfo {
    HWND windowHandle;
    QString windowTitle;
    QIcon icon;
    DWORD processId;
    QString architecture;
};

struct WindowOperationInfo {
    QString processName;
    DWORD pid = 0;
    HWND hwnd = nullptr;
    bool isValid = false;
    QString errorMessage;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

   public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

   protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
    void setVisible(bool visible) override;

   private slots:
    void onAutomaticRefreshToggled(bool enabled);
    void onRefreshIntervalChanged(double value);
    void onHideFromScreenCaptureChanged(bool enabled);
    void onRandomizeWindowTitlesChanged(bool enabled);
    void onRandomizeTrayIconChanged(bool enabled);
    void onEnableTrayIconChanged(bool enabled);
    void onMinimizeToTrayChanged(bool enabled);
    void onHideTaskbarIconChanged(bool enabled);
    void showNotification(const QString& title, const QString& message);
    void iconActivated(QSystemTrayIcon::ActivationReason reason);

   private:
    Ui::MainWindow* ui;
    QTimer* refreshTimer;
    Autohider* m_autohideWatcher;

    // System tray icon
    QSystemTrayIcon* m_trayIcon;
    QMenu* m_trayIconMenu;
    QAction* m_restoreAction;
    QAction* m_quitAction;
    bool m_isClosing;
    bool m_trayIconHintShown;

    // Cached data for filtering
    QList<WindowInfo> allWindows;
    QList<ProcessInfo> allProcesses;

    // Result structure for operations
    struct OperationResults {
        int successCount = 0;
        int failureCount = 0;
        QStringList failedItems;
    };

    // Sort state for table restoration
    struct SortState {
        bool wasEnabled = false;
        int column = -1;
        Qt::SortOrder order = Qt::AscendingOrder;
    };

    // Initialization methods
    void initializeWindow();
    void setupTables();
    void setupRefreshTimer();
    void connectSignals();
    void configureUI();
    void setupKeyboardShortcuts();
    void setupTooltips();
    void setupRefreshIntervalSpinBox();
    void setupSystemTrayIcon();
    void createTrayIcon();

    // Table setup methods
    void setupWindowsTable();
    void setupProcessesTable();
    void configureTableBehavior(QTableWidget* table);

    // Window operation methods
    void performWindowOperation(bool hideOperation);
    QSet<int> getSelectedRows();
    WindowOperationInfo extractWindowInfo(int row);
    void performSingleWindowOperation(
        int row,
        bool hideOperation,
        int& successCount,
        int& failureCount,
        QStringList& failedProcesses,
        QStringList& failureReasons
    );
    void showOperationResult(
        bool hideOperation,
        int successCount,
        int failureCount,
        const QStringList& failedProcesses,
        const QStringList& failureReasons
    );

    // Refresh methods
    void refreshCurrentTable();
    void refreshWindowsList();
    void refreshProcessList();

    // Filter methods
    void filterWindowsTable(const QString& filterText);
    void filterProcessesTable(const QString& filterText);
    void applyWindowsFilter();
    void applyProcessesFilter();
    void preserveSelectionDuringRefresh();
    QSet<HWND> getSelectedWindowHandles() const;
    QSet<DWORD> getSelectedProcessPIDs() const;
    void restoreWindowSelection(const QSet<HWND>& selectedHandles);
    void restoreProcessSelection(const QSet<DWORD>& selectedPIDs);
    SortState captureSortState(const QTableWidget* table) const;
    void temporarilyDisableSorting(QTableWidget* table) const;
    void restoreSortState(QTableWidget* table, const SortState& sortState) const;

    // Table population methods
    void populateWindowsTable(QTableWidget* table);
    void populateProcessesTable(QTableWidget* table);
    void addWindowToTable(QTableWidget* table, int row, const WindowInfo& window);
    void addProcessToTable(QTableWidget* table, int row, const ProcessInfo& process);
    QTableWidgetItem* createReadOnlyItem(const QString& text = QString()) const;

    // Data collection methods
    QList<ProcessInfo> getRunningProcesses();
    QList<WindowInfo> getRunningWindows();
    bool shouldSkipProcess(DWORD pid) const;
    std::optional<ProcessInfo> createProcessInfo(const PROCESSENTRY32& pe32);
    QIcon getProcessIcon(const QString& executablePath);
    static bool shouldIncludeWindow(HWND hwnd);
    static std::optional<WindowInfo> createWindowInfo(HWND hwnd);
    static QIcon getWindowIcon(DWORD pid);

    // Utility methods
    QTableWidget* getCurrentTable() const;
    bool isWindowsTabActive() const;
    void setRowBackgroundColor(QTableWidget* table, int row, const QColor& color);
    void updateRowColors();

    // Static callback for EnumWindows
    static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam);

    // Store original title for restoration
    QString originalMainWindowTitle;

    // Constants
    static const QColor kHiddenWindowBackgroundColor;
};

const QString kVersion = "1.3.0";
