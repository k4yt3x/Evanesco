#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "aboutdialog.h"
#include "prefdialog.h"
#include "procutils.h"
#include "randutils.h"
#include "settings.h"
#include "windowhider.h"

// Define the constant for hidden window background color
const QColor MainWindow::kHiddenWindowBackgroundColor(128, 0, 128, 100);

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    setWindowTitle("Evanesco " + kVersion);

    // Setup the tables
    setupWindowsTable();
    setupProcessesTable();

    // Initialize refresh timer
    refreshTimer = new QTimer(this);
    refreshTimer->setSingleShot(false);
    connect(refreshTimer, &QTimer::timeout, this, &MainWindow::refreshCurrentTable);

    // Connect signals and slots
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::close);
    connect(ui->actionRefreshCurrentTable, &QAction::triggered, this, &MainWindow::refreshCurrentTable);
    connect(ui->actionAbout, &QAction::triggered, this, [&]() {
        AboutDialog aboutDialog(this, kVersion);
        aboutDialog.exec();
    });

    // Connect to Settings signals for refresh functionality
    Settings* settings = Settings::instance();
    connect(settings, &Settings::autoRefreshChanged, this, &MainWindow::onAutomaticRefreshToggled);
    connect(settings, &Settings::refreshIntervalChanged, this, &MainWindow::onRefreshIntervalChanged);
    connect(settings, &Settings::hideFromScreenCaptureChanged, this, &MainWindow::onHideFromScreenCaptureChanged);
    connect(settings, &Settings::randomizeWindowTitlesChanged, this, &MainWindow::onRandomizeWindowTitlesChanged);
    connect(settings, &Settings::hideTaskbarIconChanged, this, &MainWindow::onHideTaskbarIconChanged);
    connect(settings, &Settings::hideTargetTaskbarIconsChanged, this, &MainWindow::onHideTargetTaskbarIconsChanged);

    // Connect auto refresh changed signal to update menu action
    connect(settings, &Settings::autoRefreshChanged, ui->actionAutoRefreshTables, &QAction::setChecked);

    // Connect preferences action
    connect(ui->actionPreferences, &QAction::triggered, this, [this]() {
        PrefDialog prefDialog(this);
        prefDialog.exec();
    });

    // Connect auto refresh menu action
    connect(ui->actionAutoRefreshTables, &QAction::toggled, this, [this](bool checked) {
        Settings::instance()->setAutoRefresh(checked);
    });

    // Initialize menu action state
    ui->actionAutoRefreshTables->setChecked(settings->autoRefresh());
    connect(ui->windowsListRefreshPushButton, &QPushButton::clicked, this, &MainWindow::refreshCurrentTable);
    connect(ui->processesListRefreshPushButton, &QPushButton::clicked, this, &MainWindow::refreshCurrentTable);
    connect(ui->hidePushButton, &QPushButton::clicked, this, [&]() { performWindowOperation(true); });
    connect(ui->unhidePushButton, &QPushButton::clicked, this, [&]() { performWindowOperation(false); });

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MainWindow::refreshCurrentTable);
    connect(ui->windowTitleFilterLineEdit, &QLineEdit::textChanged, this, &MainWindow::applyWindowsFilter);
    connect(ui->processNameFilterLineEdit, &QLineEdit::textChanged, this, &MainWindow::applyProcessesFilter);

    // Initialize refresh timer based on settings
    if (settings->autoRefresh()) {
        onAutomaticRefreshToggled(true);
    }

    // Initialize screen capture hiding
    onHideFromScreenCaptureChanged(settings->hideFromScreenCapture());

    // Store original title and initialize randomization
    originalMainWindowTitle = this->windowTitle();
    onRandomizeWindowTitlesChanged(settings->randomizeWindowTitles());

    // Initialize taskbar icon hiding
    onHideTaskbarIconChanged(settings->hideTaskbarIcon());

    // Initial table population
    refreshCurrentTable();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::onAutomaticRefreshToggled(bool enabled) {
    if (enabled) {
        double interval = Settings::instance()->refreshInterval();
        refreshTimer->start(static_cast<int>(interval * 1000));
    } else {
        refreshTimer->stop();
    }
}

void MainWindow::onRefreshIntervalChanged(double value) {
    if (Settings::instance()->autoRefresh()) {
        refreshTimer->start(static_cast<int>(value * 1000));
    }
}

void MainWindow::onHideFromScreenCaptureChanged(bool enabled) {
    HWND hwnd = reinterpret_cast<HWND>(this->winId());
    if (enabled) {
        SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
    } else {
        SetWindowDisplayAffinity(hwnd, WDA_NONE);
    }
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);

    // Apply screen capture hiding when window is shown
    Settings* settings = Settings::instance();
    HWND hwnd = reinterpret_cast<HWND>(this->winId());
    if (settings->hideFromScreenCapture()) {
        SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
    } else {
        SetWindowDisplayAffinity(hwnd, WDA_NONE);
    }
}

void MainWindow::onRandomizeWindowTitlesChanged(bool enabled) {
    if (enabled) {
        this->setWindowTitle(RandUtils::generateRandomTitle());
    } else {
        this->setWindowTitle(originalMainWindowTitle);
    }
}

void MainWindow::onHideTaskbarIconChanged(bool enabled) {
    Qt::WindowFlags flags = this->windowFlags();
    if (enabled) {
        flags |= Qt::Tool;
    } else {
        flags &= ~Qt::Tool;
    }
    this->setWindowFlags(flags);
    // Re-show the window to apply flag changes
    this->show();
}

void MainWindow::onHideTargetTaskbarIconsChanged(bool enabled) {
    // This setting affects future window operations
    // No immediate UI changes needed as it's applied during injection
}

void MainWindow::setupWindowsTable() {
    // Set up table columns
    ui->windowsTableWidget->setColumnCount(5);

    QStringList headers;
    headers << "Icon" << "Window Title" << "Handle" << "PID" << "Architecture";
    ui->windowsTableWidget->setHorizontalHeaderLabels(headers);

    // Set column widths
    ui->windowsTableWidget->setColumnWidth(0, 50);   // Icon column
    ui->windowsTableWidget->setColumnWidth(1, 320);  // Window Title column
    ui->windowsTableWidget->setColumnWidth(2, 100);  // Handle column
    ui->windowsTableWidget->setColumnWidth(3, 80);   // PID column
    ui->windowsTableWidget->setColumnWidth(4, 100);  // Architecture column

    // Stretch the window title column
    ui->windowsTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->windowsTableWidget->sortItems(1, Qt::AscendingOrder);
}

void MainWindow::setupProcessesTable() {
    // Set up table columns
    ui->processesTableWidget->setColumnCount(4);

    QStringList headers;
    headers << "Icon" << "Process Name" << "PID" << "Architecture";
    ui->processesTableWidget->setHorizontalHeaderLabels(headers);

    // Set column widths
    ui->processesTableWidget->setColumnWidth(0, 50);   // Icon column
    ui->processesTableWidget->setColumnWidth(1, 200);  // Process Name column
    ui->processesTableWidget->setColumnWidth(2, 80);   // PID column
    ui->processesTableWidget->setColumnWidth(3, 100);  // Architecture column

    // Stretch the process name column
    ui->processesTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->processesTableWidget->sortItems(1, Qt::AscendingOrder);
}

void MainWindow::performWindowOperation(bool hideOperation) {
    QSet<int> selectedRows = getSelectedRows();
    if (selectedRows.isEmpty()) {
        QString operationName = hideOperation ? "hide" : "unhide";
        QMessageBox::warning(
            this, "No Selection", QString("Please select one or more windows to %1.").arg(operationName)
        );
        return;
    }

    int successCount = 0;
    int failureCount = 0;
    QStringList failedProcesses;
    QStringList failureReasons;

    for (int row : selectedRows) {
        performSingleWindowOperation(row, hideOperation, successCount, failureCount, failedProcesses, failureReasons);
    }

    showOperationResult(hideOperation, successCount, failureCount, failedProcesses, failureReasons);
}

QSet<int> MainWindow::getSelectedRows() {
    QTableWidget* currentTable = getCurrentTable();
    QList<QTableWidgetItem*> selectedItems = currentTable->selectedItems();

    QSet<int> selectedRows;
    for (QTableWidgetItem* item : selectedItems) {
        selectedRows.insert(item->row());
    }
    return selectedRows;
}

WindowOperationInfo MainWindow::extractWindowInfo(int row) {
    WindowOperationInfo info;

    if (isWindowsTabActive()) {
        QTableWidgetItem* handleItem = ui->windowsTableWidget->item(row, 2);
        QTableWidgetItem* titleItem = ui->windowsTableWidget->item(row, 1);

        if (!handleItem || !titleItem) {
            info.errorMessage = "Window data missing";
            return info;
        }

        info.processName = titleItem->text();

        bool handleOk;
        quintptr handleValue = handleItem->text().toULongLong(&handleOk, 16);
        if (!handleOk || handleValue == 0) {
            info.errorMessage = "Invalid window handle format";
            return info;
        }

        info.hwnd = reinterpret_cast<HWND>(handleValue);
        if (!IsWindow(info.hwnd)) {
            info.errorMessage = "Window has been closed or is no longer valid";
            return info;
        }

        GetWindowThreadProcessId(info.hwnd, &info.pid);
        info.isValid = true;
    } else {
        QTableWidgetItem* pidItem = ui->processesTableWidget->item(row, 2);
        QTableWidgetItem* nameItem = ui->processesTableWidget->item(row, 1);

        if (!pidItem || !nameItem) {
            info.errorMessage = "Process data missing";
            return info;
        }

        info.processName = nameItem->text();

        bool ok;
        info.pid = pidItem->text().toULong(&ok);
        if (!ok || info.pid == 0) {
            info.errorMessage = "Invalid process ID";
            return info;
        }

        info.isValid = true;
    }

    return info;
}

void MainWindow::performSingleWindowOperation(
    int row,
    bool hideOperation,
    int& successCount,
    int& failureCount,
    QStringList& failedProcesses,
    QStringList& failureReasons
) {
    WindowOperationInfo info = extractWindowInfo(row);
    if (!info.isValid) {
        failureCount++;
        failedProcesses.append(info.processName);
        failureReasons.append(info.errorMessage);
        return;
    }

    QString errorMsg;
    bool success = false;

    Settings* settings = Settings::instance();
    bool hideTaskbarIcon = settings->hideTargetTaskbarIcons();

    if (isWindowsTabActive()) {
        success = hideOperation ? WindowHider::HideWindow(info.hwnd, hideTaskbarIcon, &errorMsg)
                                : WindowHider::UnhideWindow(info.hwnd, hideTaskbarIcon, &errorMsg);
    } else {
        success = hideOperation ? WindowHider::HideProcessWindows(info.pid, hideTaskbarIcon, &errorMsg)
                                : WindowHider::UnhideProcessWindows(info.pid, hideTaskbarIcon, &errorMsg);
    }

    if (success) {
        successCount++;
        // Update row background color based on operation
        QTableWidget* table = getCurrentTable();
        if (hideOperation) {
            // Set purple background for hidden windows
            setRowBackgroundColor(table, row, kHiddenWindowBackgroundColor);
        } else {
            // Clear background for unhidden windows
            setRowBackgroundColor(table, row, QColor());
        }
    } else {
        failureCount++;
        failedProcesses.append(info.processName);
        QString operationName = hideOperation ? "hide" : "unhide";
        failureReasons.append(errorMsg.isEmpty() ? QString("Failed to %1 window").arg(operationName) : errorMsg);
    }
}

void MainWindow::showOperationResult(
    bool hideOperation,
    int successCount,
    int failureCount,
    const QStringList& failedProcesses,
    const QStringList& failureReasons
) {
    QString operationName = hideOperation ? "hide" : "unhide";
    QString operationNamePast = hideOperation ? "hidden" : "unhidden";

    if (failureCount == 0) {
        // Success case - only show in status bar
        QString statusMessage = QString("Successfully %1 %2 window(s)!").arg(operationNamePast).arg(successCount);
        // Show for 5 seconds
        statusBar()->showMessage(statusMessage, 5000);
        return;
    }

    // Error case - show brief error in status bar AND popup with details
    QString statusMessage;
    QString failureDetails;
    for (int i = 0; i < failedProcesses.size() && i < failureReasons.size(); ++i) {
        failureDetails += QString("• %1: %2").arg(failedProcesses[i], failureReasons[i]);

        // Add two newlines except for the last item
        if (i < failedProcesses.size() - 1) {
            failureDetails += "\n\n";
        }
    }

    if (successCount > 0) {
        statusMessage =
            QString("Partial success: %1 %2, %3 failed").arg(successCount).arg(operationNamePast).arg(failureCount);
        QString message = QString("%1 %2 window(s), failed to %3 %4 window(s):\n\n")
                              .arg(operationNamePast.at(0).toUpper() + operationNamePast.mid(1))
                              .arg(successCount)
                              .arg(operationName)
                              .arg(failureCount);
        statusBar()->showMessage(statusMessage, 5000);
        QMessageBox::warning(this, "Partial Success", message + failureDetails);
    } else {
        statusMessage = QString("Failed to %1 %2 window(s)").arg(operationName).arg(failureCount);
        QString message = QString("Failed to %1 the following windows:\n\n").arg(operationName);
        statusBar()->showMessage(statusMessage, 5000);
        QMessageBox::critical(this, "Operation Failed", message + failureDetails);
    }
}

void MainWindow::refreshCurrentTable() {
    if (isWindowsTabActive()) {
        refreshWindowsList();
    } else {
        refreshProcessList();
    }
}

void MainWindow::refreshWindowsList() {
    // Store current selection before refresh
    QTableWidget* table = ui->windowsTableWidget;
    QSet<HWND> selectedHandles = getSelectedWindowHandles();
    SortState sortState = captureSortState(table);

    // Get windows list and cache it
    allWindows = getRunningWindows();

    // Apply current filter
    applyWindowsFilter();

    // Restore selection and sort state
    restoreWindowSelection(selectedHandles);
    restoreSortState(table, sortState);
}

void MainWindow::refreshProcessList() {
    // Store current selection before refresh
    QTableWidget* table = ui->processesTableWidget;
    QSet<DWORD> selectedPIDs = getSelectedProcessPIDs();
    SortState sortState = captureSortState(table);

    // Get process list and cache it
    allProcesses = getRunningProcesses();

    // Apply current filter
    applyProcessesFilter();

    // Restore selection and sort state
    restoreProcessSelection(selectedPIDs);
    restoreSortState(table, sortState);
}

QSet<HWND> MainWindow::getSelectedWindowHandles() const {
    QSet<HWND> selectedHandles;
    QTableWidget* table = ui->windowsTableWidget;
    QModelIndexList selectedRows = table->selectionModel()->selectedRows();

    for (const QModelIndex& index : selectedRows) {
        int row = index.row();
        if (row < table->rowCount()) {
            // Handle is in column 2
            QTableWidgetItem* handleItem = table->item(row, 2);
            if (handleItem) {
                QString handleText = handleItem->text();
                bool ok;
                HWND handle = reinterpret_cast<HWND>(handleText.toULongLong(&ok, 16));
                if (ok) {
                    selectedHandles.insert(handle);
                }
            }
        }
    }
    return selectedHandles;
}

QSet<DWORD> MainWindow::getSelectedProcessPIDs() const {
    QSet<DWORD> selectedPIDs;
    QTableWidget* table = ui->processesTableWidget;
    QModelIndexList selectedRows = table->selectionModel()->selectedRows();

    for (const QModelIndex& index : selectedRows) {
        int row = index.row();
        if (row < table->rowCount()) {
            // PID is in column 2
            QTableWidgetItem* pidItem = table->item(row, 2);
            if (pidItem) {
                QString pidText = pidItem->text();
                bool ok;
                DWORD pid = pidText.toULong(&ok);
                if (ok) {
                    selectedPIDs.insert(pid);
                }
            }
        }
    }
    return selectedPIDs;
}

void MainWindow::restoreWindowSelection(const QSet<HWND>& selectedHandles) {
    if (selectedHandles.isEmpty()) {
        return;
    }

    QTableWidget* table = ui->windowsTableWidget;
    QItemSelectionModel* selectionModel = table->selectionModel();
    selectionModel->clearSelection();

    for (int row = 0; row < table->rowCount(); ++row) {
        // Handle is in column 2
        QTableWidgetItem* handleItem = table->item(row, 2);
        if (handleItem) {
            QString handleText = handleItem->text();
            bool ok;
            HWND handle = reinterpret_cast<HWND>(handleText.toULongLong(&ok, 16));
            if (ok && selectedHandles.contains(handle)) {
                selectionModel->select(
                    table->model()->index(row, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows
                );
            }
        }
    }
}

void MainWindow::restoreProcessSelection(const QSet<DWORD>& selectedPIDs) {
    if (selectedPIDs.isEmpty()) {
        return;
    }

    QTableWidget* table = ui->processesTableWidget;
    QItemSelectionModel* selectionModel = table->selectionModel();
    selectionModel->clearSelection();

    for (int row = 0; row < table->rowCount(); ++row) {
        // PID is in column 2
        QTableWidgetItem* pidItem = table->item(row, 2);
        if (pidItem) {
            QString pidText = pidItem->text();
            bool ok;
            DWORD pid = pidText.toULong(&ok);
            if (ok && selectedPIDs.contains(pid)) {
                selectionModel->select(
                    table->model()->index(row, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows
                );
            }
        }
    }
}

MainWindow::SortState MainWindow::captureSortState(const QTableWidget* table) const {
    SortState sortState;
    QHeaderView* header = table->horizontalHeader();
    sortState.wasEnabled = table->isSortingEnabled();
    sortState.column = header->sortIndicatorSection();
    sortState.order = header->sortIndicatorOrder();
    return sortState;
}

void MainWindow::temporarilyDisableSorting(QTableWidget* table) const {
    table->setSortingEnabled(false);
}

void MainWindow::restoreSortState(QTableWidget* table, const SortState& sortState) const {
    if (sortState.wasEnabled) {
        table->setSortingEnabled(true);
        if (sortState.column >= 0) {
            table->sortItems(sortState.column, sortState.order);
        }
    }
}

QList<ProcessInfo> MainWindow::getRunningProcesses() {
    QList<ProcessInfo> processes;

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return processes;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            // Skip system processes and current process
            if (pe32.th32ProcessID <= 4) {
                continue;
            }

            // Get the main window handle for this process
            HWND mainWindow = ProcUtils::getMainWindowHandle(pe32.th32ProcessID);
            ProcessInfo info;
            info.pid = pe32.th32ProcessID;
            info.processName = QString::fromWCharArray(pe32.szExeFile);
            info.executablePath = ProcUtils::getProcessExecutablePath(pe32.th32ProcessID);
            info.architecture = ProcUtils::getProcessArchitecture(pe32.th32ProcessID);
            info.icon = getProcessIcon(info.executablePath);
            info.windowHandle = mainWindow;

            // Only add if we have valid basic information
            if (info.pid > 0 && !info.processName.isEmpty()) {
                processes.append(info);
            }

        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return processes;
}

QIcon MainWindow::getProcessIcon(const QString& executablePath) {
    if (executablePath.isEmpty()) {
        return QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    }

    // Try to get the file icon
    QFileIconProvider iconProvider;
    QIcon icon = iconProvider.icon(QFileInfo(executablePath));

    if (icon.isNull()) {
        return QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    }

    return icon;
}

BOOL CALLBACK MainWindow::EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
    QList<WindowInfo>* windowsList = reinterpret_cast<QList<WindowInfo>*>(lParam);

    // Only include visible windows with titles
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    wchar_t title[256];
    if (GetWindowTextW(hwnd, title, sizeof(title) / sizeof(wchar_t)) == 0) {
        return TRUE;
    }

    // Check if it's a main window (has no owner and is not a tool window)
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);

    if (!(style & WS_VISIBLE) || (exStyle & WS_EX_TOOLWINDOW) || GetWindow(hwnd, GW_OWNER) != NULL ||
        (style & WS_CHILD)) {
        return TRUE;
    }

    // Get process ID
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);

    // Skip system processes and the current process
    if (pid <= 4) {
        return TRUE;
    }

    WindowInfo info;
    info.windowHandle = hwnd;
    info.windowTitle = QString::fromWCharArray(title);
    info.processId = pid;
    info.architecture = ProcUtils::getProcessArchitecture(pid);

    // Get process icon directly
    QFileIconProvider iconProvider;
    QString execPath = ProcUtils::getProcessExecutablePath(pid);
    if (!execPath.isEmpty()) {
        info.icon = iconProvider.icon(QFileInfo(execPath));
    } else {
        info.icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    }

    windowsList->append(info);
    return TRUE;
}

QList<WindowInfo> MainWindow::getRunningWindows() {
    QList<WindowInfo> windows;
    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&windows));
    return windows;
}

QTableWidget* MainWindow::getCurrentTable() const {
    return isWindowsTabActive() ? ui->windowsTableWidget : ui->processesTableWidget;
}

bool MainWindow::isWindowsTabActive() const {
    return ui->tabWidget->currentIndex() == 0;
}

void MainWindow::setRowBackgroundColor(QTableWidget* table, int row, const QColor& color) {
    for (int col = 0; col < table->columnCount(); ++col) {
        QTableWidgetItem* item = table->item(row, col);
        if (item) {
            if (color.isValid()) {
                item->setBackground(color);
            } else {
                // Clear the background by using the table's default palette
                item->setData(Qt::BackgroundRole, QVariant());
            }
        }
    }
}

void MainWindow::updateRowColors() {
    if (!isWindowsTabActive()) {
        // Only applies to windows table
        return;
    }

    QTableWidget* table = ui->windowsTableWidget;

    for (int row = 0; row < table->rowCount(); ++row) {
        // Handle is in column 2
        QTableWidgetItem* handleItem = table->item(row, 2);
        if (handleItem) {
            QString handleText = handleItem->text();
            bool ok;
            HWND windowHandle = reinterpret_cast<HWND>(handleText.toULongLong(&ok, 16));
            if (ok) {
                DWORD affinity = 0;
                if (GetWindowDisplayAffinity(windowHandle, &affinity)) {
                    if (affinity == WDA_EXCLUDEFROMCAPTURE) {
                        setRowBackgroundColor(table, row, kHiddenWindowBackgroundColor);
                    } else {
                        // Clear background
                        setRowBackgroundColor(table, row, QColor());
                    }
                }
            }
        }
    }
}

void MainWindow::applyWindowsFilter() {
    QString filterText = ui->windowTitleFilterLineEdit->text().trimmed();
    filterWindowsTable(filterText);
}

void MainWindow::applyProcessesFilter() {
    QString filterText = ui->processNameFilterLineEdit->text().trimmed();
    filterProcessesTable(filterText);
}

void MainWindow::filterWindowsTable(const QString& filterText) {
    QTableWidget* table = ui->windowsTableWidget;

    // Store current sort state
    bool sortingWasEnabled = table->isSortingEnabled();
    int currentSortColumn = -1;
    Qt::SortOrder currentSortOrder = Qt::AscendingOrder;

    if (sortingWasEnabled) {
        QHeaderView* header = table->horizontalHeader();
        currentSortColumn = header->sortIndicatorSection();
        currentSortOrder = header->sortIndicatorOrder();
    }

    // Temporarily disable sorting to prevent data misalignment during population
    table->setSortingEnabled(false);

    // Clear existing items
    table->setRowCount(0);

    // Filter windows based on title
    QList<WindowInfo> filteredWindows;
    if (filterText.isEmpty()) {
        filteredWindows = allWindows;
    } else {
        for (const WindowInfo& window : allWindows) {
            if (window.windowTitle.contains(filterText, Qt::CaseInsensitive)) {
                filteredWindows.append(window);
            }
        }
    }

    // Populate table with filtered data
    table->setRowCount(filteredWindows.size());

    for (int i = 0; i < filteredWindows.size(); ++i) {
        const WindowInfo& window = filteredWindows[i];

        // Icon column
        QTableWidgetItem* iconItem = new QTableWidgetItem();
        iconItem->setIcon(window.icon);
        iconItem->setFlags(iconItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(i, 0, iconItem);

        // Window title column
        QTableWidgetItem* titleItem = new QTableWidgetItem(window.windowTitle);
        titleItem->setFlags(titleItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(i, 1, titleItem);

        // Handle column
        QTableWidgetItem* handleItem =
            new QTableWidgetItem(QString("%1").arg(reinterpret_cast<quintptr>(window.windowHandle), 0, 16).toUpper());
        handleItem->setFlags(handleItem->flags() & ~Qt::ItemIsEditable);
        handleItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(i, 2, handleItem);

        // PID column
        QTableWidgetItem* pidItem = new QTableWidgetItem(QString::number(window.processId));
        pidItem->setFlags(pidItem->flags() & ~Qt::ItemIsEditable);
        pidItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(i, 3, pidItem);

        // Architecture column
        QTableWidgetItem* archItem = new QTableWidgetItem(window.architecture);
        archItem->setFlags(archItem->flags() & ~Qt::ItemIsEditable);
        archItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(i, 4, archItem);

        // Check window display affinity and set purple background if excluded from capture
        DWORD affinity = 0;
        if (GetWindowDisplayAffinity(window.windowHandle, &affinity)) {
            if (affinity == WDA_EXCLUDEFROMCAPTURE) {
                setRowBackgroundColor(table, i, kHiddenWindowBackgroundColor);
            }
        }
    }

    // Re-enable sorting and restore previous sort state
    table->setSortingEnabled(sortingWasEnabled);
    if (sortingWasEnabled && currentSortColumn != -1) {
        table->sortItems(currentSortColumn, currentSortOrder);
    } else if (sortingWasEnabled) {
        // Default to window title if no previous sort state
        table->sortItems(1, Qt::AscendingOrder);
    }
}

void MainWindow::filterProcessesTable(const QString& filterText) {
    QTableWidget* table = ui->processesTableWidget;

    // Store current sort state
    bool sortingWasEnabled = table->isSortingEnabled();
    int currentSortColumn = -1;
    Qt::SortOrder currentSortOrder = Qt::AscendingOrder;

    if (sortingWasEnabled) {
        QHeaderView* header = table->horizontalHeader();
        currentSortColumn = header->sortIndicatorSection();
        currentSortOrder = header->sortIndicatorOrder();
    }

    // Temporarily disable sorting to prevent data misalignment during population
    table->setSortingEnabled(false);

    // Clear existing items
    table->setRowCount(0);

    // Filter processes based on name
    QList<ProcessInfo> filteredProcesses;
    if (filterText.isEmpty()) {
        filteredProcesses = allProcesses;
    } else {
        for (const ProcessInfo& process : allProcesses) {
            if (process.processName.contains(filterText, Qt::CaseInsensitive)) {
                filteredProcesses.append(process);
            }
        }
    }

    // Populate table with filtered data
    table->setRowCount(filteredProcesses.size());

    for (int i = 0; i < filteredProcesses.size(); ++i) {
        const ProcessInfo& proc = filteredProcesses[i];

        // Icon column
        QTableWidgetItem* iconItem = new QTableWidgetItem();
        iconItem->setIcon(proc.icon);
        iconItem->setFlags(iconItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(i, 0, iconItem);

        // Process name column
        QTableWidgetItem* nameItem = new QTableWidgetItem(proc.processName);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(i, 1, nameItem);

        // PID column
        QTableWidgetItem* pidItem = new QTableWidgetItem(QString::number(proc.pid));
        pidItem->setFlags(pidItem->flags() & ~Qt::ItemIsEditable);
        pidItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(i, 2, pidItem);

        // Architecture column
        QTableWidgetItem* archItem = new QTableWidgetItem(proc.architecture);
        archItem->setFlags(archItem->flags() & ~Qt::ItemIsEditable);
        archItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(i, 3, archItem);
    }

    // Re-enable sorting and restore previous sort state
    table->setSortingEnabled(sortingWasEnabled);
    if (sortingWasEnabled && currentSortColumn != -1) {
        table->sortItems(currentSortColumn, currentSortOrder);
    } else if (sortingWasEnabled) {
        // Default to process name if no previous sort state
        table->sortItems(1, Qt::AscendingOrder);
    }
}
