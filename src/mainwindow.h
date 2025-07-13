#pragma once
#include "procutils.h"

#include <optional>

#include <psapi.h>
#include <tlhelp32.h>
#include <windows.h>

#include <QApplication>
#include <QCheckBox>
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
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>

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

   private slots:
    void onAutomaticRefreshToggled(bool enabled);
    void onRefreshIntervalChanged(double value);
    void onTimerTimeout();

   private:
    Ui::MainWindow* ui;
    QTimer* refreshTimer;

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
    QList<QPersistentModelIndex> getSelectedIndexes(const QTableWidget* table) const;
    void restoreSelection(const QTableWidget* table, const QList<QPersistentModelIndex>& selectedIndexes) const;
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

    // Static callback for EnumWindows
    static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam);
};

const QString kVersion = "1.0.0";
