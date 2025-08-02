#include "autohider.h"

#include <tlhelp32.h>
#include <windows.h>

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QMetaObject>
#include <QThread>

#include "procutils.h"
#include "settings.h"
#include "windowhider.h"

Autohider::Autohider(QObject* parent) : QObject(parent), m_workerThread(nullptr), m_isRunning(false) {
    // Create worker thread
    m_workerThread = new WmiWorkerThread(this);

    // Connect worker thread signals
    connect(m_workerThread, &WmiWorkerThread::processCreated, this, &Autohider::onProcessCreated);
    connect(m_workerThread, &WmiWorkerThread::wmiError, this, &Autohider::onWmiError);
}

Autohider::~Autohider() {
    stop();
}

void Autohider::start() {
    QMutexLocker locker(&m_mutex);

    if (m_isRunning) {
        return;
    }

    m_isRunning = true;
    m_workerThread->startWatching();
}

void Autohider::stop() {
    QMutexLocker locker(&m_mutex);

    if (!m_isRunning) {
        return;
    }

    m_isRunning = false;
    m_workerThread->stopWatching();
}

bool Autohider::isRunning() const {
    QMutexLocker locker(&m_mutex);
    return m_isRunning;
}

void Autohider::setList(const QStringList& list) {
    QMutexLocker locker(&m_mutex);
    m_list = list;
}

QStringList Autohider::getList() const {
    QMutexLocker locker(&m_mutex);
    return m_list;
}

void Autohider::onProcessCreated(DWORD processId, const QString& processName, const QString& executablePath) {
    qDebug() << "Process created:" << processName;

    emit processDetected(processId, processName, executablePath);

    if (!shouldProcessBeHidden(executablePath)) {
        qDebug() << "Process not in autohide list, skipping hiding:" << processName;
        return;
    }

    // Wait for process to create windows with configurable timeout
    bool hasWindows = processHasWindows(processId);
    if (!hasWindows) {
        int maxWaitMs = Settings::instance()->maxWindowCreationWaitMs();
        if (maxWaitMs > 0) {
            QElapsedTimer timer;
            timer.start();

            while (!hasWindows && timer.elapsed() < maxWaitMs) {
                qDebug() << "Waiting for process windows to appear for PID:" << processId;
                QThread::msleep(kWindowCheckIntervalMs);
                hasWindows = processHasWindows(processId);
            }
        }
    }

    if (!hasWindows) {
        qDebug() << "Process has no windows, skipping hiding:" << processName;
        return;
    }

    QString errorMessage;
    if (!hideProcessWindows(processId, &errorMessage)) {
        emit errorOccurred(
            QString("Failed to hide process %1 (PID: %2): %3").arg(processName).arg(processId).arg(errorMessage)
        );
        return;
    }

    emit processHidden(processId, processName, executablePath);

    // Show notification if enabled
    if (Settings::instance()->autohideNotify()) {
        emit notificationRequested(
            "Process Hidden", QString("Process '%1' has been automatically hidden").arg(processName)
        );
    }
}

void Autohider::onWmiError(const QString& errorMessage) {
    emit errorOccurred(QString("WMI Error: %1").arg(errorMessage));
}

bool Autohider::shouldProcessBeHidden(const QString& executablePath) const {
    QString processName = getProcessName(executablePath);
    return isProcessNameInList(processName) || isFullPathInList(executablePath);
}

bool Autohider::shouldProcessBeHiddenFromList(const QString& executablePath, const QStringList& list) const {
    QString processName = getProcessName(executablePath);
    return isProcessNameInListFromList(processName, list) || isFullPathInListFromList(executablePath, list);
}

bool Autohider::isProcessNameInListFromList(const QString& processName, const QStringList& list) const {
    for (const QString& listEntry : list) {
        // Skip entries that look like full paths
        if (listEntry.contains('\\') || listEntry.contains('/')) {
            continue;
        }
        if (processName.compare(listEntry, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

bool Autohider::isFullPathInListFromList(const QString& fullPath, const QStringList& list) const {
    QString normalizedFullPath = normalizePath(fullPath);

    for (const QString& listEntry : list) {
        // Check if list entry is a full path
        if (listEntry.contains('\\') || listEntry.contains('/')) {
            QString normalizedListEntry = normalizePath(listEntry);

            if (normalizedFullPath == normalizedListEntry) {
                return true;
            }

            // Also check if the list entry is contained in the full path (for relative paths)
            if (normalizedFullPath.endsWith(normalizedListEntry)) {
                return true;
            }
        }
    }
    return false;
}

bool Autohider::processHasWindows(DWORD processId) const {
    HWND mainWindow = ProcUtils::getProcessMainWindowHandle(processId);
    return mainWindow != nullptr;
}

bool Autohider::hideProcessWindows(DWORD processId, QString* errorMessage) {
    return WindowHider::HideProcessWindows(processId, false, errorMessage);
}

QString Autohider::getProcessName(const QString& executablePath) const {
    QFileInfo fileInfo(executablePath);
    return fileInfo.fileName();
}

bool Autohider::isProcessNameInList(const QString& processName) const {
    QMutexLocker locker(&m_mutex);
    for (const QString& listEntry : m_list) {
        // Skip entries that look like full paths
        if (listEntry.contains('\\') || listEntry.contains('/')) {
            continue;
        }
        if (processName.compare(listEntry, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

bool Autohider::isFullPathInList(const QString& fullPath) const {
    QMutexLocker locker(&m_mutex);

    QString normalizedFullPath = normalizePath(fullPath);

    for (const QString& listEntry : m_list) {
        // Check if list entry is a full path
        if (listEntry.contains('\\') || listEntry.contains('/')) {
            QString normalizedListEntry = normalizePath(listEntry);
            qDebug() << "Normalized full path:" << normalizedFullPath;
            qDebug() << "Normalized list entry:" << normalizedListEntry;

            if (normalizedFullPath == normalizedListEntry) {
                return true;
            }

            // Also check if the list entry is contained in the full path (for relative paths)
            if (normalizedFullPath.endsWith(normalizedListEntry)) {
                return true;
            }
        }
    }
    return false;
}

QString Autohider::normalizePath(const QString& path) const {
    if (path.isEmpty()) {
        return path;
    }

    // Clean the path, convert backslashes to forward slashes, and make lowercase
    QString normalized = QDir::cleanPath(path).replace('\\', '/').toLower();

    // Remove trailing slash if present (except for root paths)
    if (normalized.length() > 1 && normalized.endsWith('/')) {
        normalized.chop(1);
    }

    return normalized;
}

void Autohider::hideExistingProcesses() {
    // Get a copy of the list to avoid holding the mutex during the entire operation
    QStringList listCopy;
    {
        QMutexLocker locker(&m_mutex);
        listCopy = m_list;
    }

    if (listCopy.isEmpty()) {
        qDebug() << "No processes in autohide list, skipping existing process hiding";
        return;
    }

    qDebug() << "Searching for existing processes to hide...";

    // Get all running processes using ProcUtils
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        qDebug() << "Failed to create process snapshot";
        return;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        qDebug() << "Failed to get first process";
        return;
    }

    int hiddenCount = 0;
    int checkedCount = 0;

    do {
        DWORD processId = pe32.th32ProcessID;

        // Skip system processes
        if (processId <= 4) {
            continue;
        }

        checkedCount++;

        // Get executable path
        QString executablePath = ProcUtils::getProcessExecutablePath(processId);
        if (executablePath.isEmpty()) {
            continue;
        }

        // Check if this process should be hidden (use copy to avoid mutex issues)
        if (shouldProcessBeHiddenFromList(executablePath, listCopy)) {
            QString processName = getProcessName(executablePath);

            // Check if process has windows (wait with timeout like in onProcessCreated)
            bool hasWindows = processHasWindows(processId);
            if (!hasWindows) {
                int maxWaitMs = Settings::instance()->maxWindowCreationWaitMs();
                if (maxWaitMs > 0) {
                    QElapsedTimer timer;
                    timer.start();

                    while (!hasWindows && timer.elapsed() < maxWaitMs) {
                        QThread::msleep(kWindowCheckIntervalMs);
                        hasWindows = processHasWindows(processId);
                    }
                }
            }

            if (hasWindows) {
                QString errorMessage;
                if (hideProcessWindows(processId, &errorMessage)) {
                    hiddenCount++;
                    qDebug() << "Hidden existing process:" << processName << "(PID:" << processId << ")";

                    // Emit signals for consistency
                    emit processHidden(processId, processName, executablePath);

                    // Show notification if enabled
                    if (Settings::instance()->autohideNotify()) {
                        emit notificationRequested(
                            "Process Hidden", QString("Existing process '%1' has been hidden").arg(processName)
                        );
                    }
                } else {
                    qDebug() << "Failed to hide existing process:" << processName << "Error:" << errorMessage;
                }
            }
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);

    qDebug() << "Finished checking existing processes. Checked:" << checkedCount << "Hidden:" << hiddenCount;
}

// WmiWorkerThread Implementation
WmiWorkerThread::WmiWorkerThread(QObject* parent)
    : QThread(parent),
      m_pSvc(nullptr),
      m_pLoc(nullptr),
      m_pUnsecApp(nullptr),
      m_pStubUnk(nullptr),
      m_pSink(nullptr),
      m_shouldStop(false),
      m_stopEvent(nullptr) {}

WmiWorkerThread::~WmiWorkerThread() {
    stopWatching();

    if (m_stopEvent) {
        CloseHandle(m_stopEvent);
        m_stopEvent = nullptr;
    }
}

void WmiWorkerThread::startWatching() {
    if (isRunning()) {
        return;
    }

    // Create stop event if it doesn't exist
    if (!m_stopEvent) {
        m_stopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (!m_stopEvent) {
            DWORD error = GetLastError();
            emit wmiError(QString("Failed to create stop event, error: %1").arg(error));
            return;
        }
    }

    // Reset the event and start flag
    ResetEvent(m_stopEvent);
    m_shouldStop = false;
    start();
}

void WmiWorkerThread::stopWatching() {
    m_shouldStop = true;

    // Signal the stop event to wake up the event loop
    if (m_stopEvent) {
        SetEvent(m_stopEvent);
    }

    if (isRunning()) {
        qDebug() << "Waiting for WMI worker thread to finish...";
        wait(3000);  // Wait up to 3 seconds for thread to finish
        if (isRunning()) {
            qDebug() << "WMI worker thread did not finish gracefully, terminating...";
            terminate();
            wait(1000);
        }
    }
}

void WmiWorkerThread::run() {
    // Initialize COM in STA mode for this thread
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        emit wmiError(QString("Failed to initialize COM: 0x%1").arg(static_cast<uint32_t>(hr), 8, 16, QChar('0')));
        return;
    }

    // Initialize COM security
    hr = CoInitializeSecurity(
        nullptr,                      // Security descriptor
        -1,                           // COM authentication
        nullptr,                      // Authentication services
        nullptr,                      // Reserved
        RPC_C_AUTHN_LEVEL_NONE,       // Default authentication
        RPC_C_IMP_LEVEL_IMPERSONATE,  // Default Impersonation
        nullptr,                      // Authentication info
        EOAC_NONE,                    // Additional capabilities
        nullptr                       // Reserved
    );

    if (FAILED(hr) && hr != RPC_E_TOO_LATE) {
        emit wmiError(
            QString("Failed to initialize COM security: 0x%1").arg(static_cast<uint32_t>(hr), 8, 16, QChar('0'))
        );
        CoUninitialize();
        return;
    }

    if (initializeWmi()) {
        processWmiEvents();
    }

    cleanupWmi();
    CoUninitialize();
}

bool WmiWorkerThread::initializeWmi() {
    HRESULT hr;

    // Create WMI locator
    hr = CoCreateInstance(
        CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, reinterpret_cast<LPVOID*>(&m_pLoc)
    );

    if (FAILED(hr)) {
        emit wmiError(QString("Failed to create WbemLocator: 0x%1").arg(static_cast<uint32_t>(hr), 8, 16, QChar('0')));
        return false;
    }

    // Connect to WMI
    hr = m_pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),
        nullptr,  // User name
        nullptr,  // User password
        nullptr,  // Locale
        0,        // Security flags
        nullptr,  // Authority
        nullptr,  // Context object
        &m_pSvc
    );

    if (FAILED(hr)) {
        emit wmiError(QString("Failed to connect to WMI: 0x%1").arg(static_cast<uint32_t>(hr), 8, 16, QChar('0')));
        return false;
    }

    // Set security levels on the proxy
    hr = CoSetProxyBlanket(
        m_pSvc,                       // Indicates the proxy to set
        RPC_C_AUTHN_WINNT,            // RPC_C_AUTHN_xxx
        RPC_C_AUTHZ_NONE,             // RPC_C_AUTHZ_xxx
        nullptr,                      // Server principal name
        RPC_C_AUTHN_LEVEL_CALL,       // RPC_C_AUTHN_LEVEL_xxx
        RPC_C_IMP_LEVEL_IMPERSONATE,  // RPC_C_IMP_LEVEL_xxx
        nullptr,                      // client identity
        EOAC_NONE                     // proxy capabilities
    );

    if (FAILED(hr)) {
        emit wmiError(QString("Failed to set proxy blanket: 0x%1").arg(static_cast<uint32_t>(hr), 8, 16, QChar('0')));
        return false;
    }

    return setupEventSink();
}

bool WmiWorkerThread::setupEventSink() {
    HRESULT hr;

    // Create an unsecured apartment for the event sink
    hr = CoCreateInstance(
        CLSID_UnsecuredApartment,
        nullptr,
        CLSCTX_LOCAL_SERVER,
        IID_IUnsecuredApartment,
        reinterpret_cast<void**>(&m_pUnsecApp)
    );

    if (FAILED(hr)) {
        emit wmiError(
            QString("Failed to create unsecured apartment: 0x%1").arg(static_cast<uint32_t>(hr), 8, 16, QChar('0'))
        );
        return false;
    }

    // Create event sink
    m_pSink = new WmiEventSink(this);
    m_pSink->AddRef();

    // Create a stub for the event sink
    hr = m_pUnsecApp->CreateObjectStub(m_pSink, &m_pStubUnk);
    if (FAILED(hr)) {
        emit wmiError(QString("Failed to create object stub: 0x%1").arg(static_cast<uint32_t>(hr), 8, 16, QChar('0')));
        return false;
    }

    // Register for process creation events
    hr = m_pSvc->ExecNotificationQueryAsync(
        _bstr_t(L"WQL"),
        _bstr_t(L"SELECT * FROM __InstanceCreationEvent WITHIN 1 WHERE TargetInstance ISA 'Win32_Process'"),
        WBEM_FLAG_SEND_STATUS,
        nullptr,
        m_pSink
    );

    if (FAILED(hr)) {
        emit wmiError(
            QString("Failed to register for process events: 0x%1").arg(static_cast<uint32_t>(hr), 8, 16, QChar('0'))
        );
        return false;
    }

    return true;
}

void WmiWorkerThread::processWmiEvents() {
    if (!m_stopEvent) {
        emit wmiError("Stop event not initialized");
        return;
    }

    qDebug() << "WMI event loop started";

    while (!m_shouldStop) {
        // Wait for either COM messages or stop event
        // This is much more efficient than polling with timeouts
        DWORD waitResult = MsgWaitForMultipleObjects(
            1,             // Number of handles to wait for
            &m_stopEvent,  // Array of handles (just stop event)
            FALSE,         // Wait for any object (not all)
            INFINITE,      // Wait indefinitely
            QS_ALLINPUT    // Wake up for any message
        );

        if (waitResult == WAIT_OBJECT_0) {
            // Stop event was signaled
            qDebug() << "WMI event loop stop signal received";
            break;
        } else if (waitResult == WAIT_OBJECT_0 + 1) {
            // Messages are available - process them
            MSG msg;
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        } else if (waitResult == WAIT_FAILED) {
            // Error occurred
            DWORD error = GetLastError();
            emit wmiError(QString("MsgWaitForMultipleObjects failed with error: %1").arg(error));
            qDebug() << "WMI event loop failed with error:" << error;
            break;
        } else {
            // This should not happen with INFINITE timeout, but handle it gracefully
            qDebug() << "WMI event loop unexpected wait result:" << waitResult;
        }
    }

    qDebug() << "WMI event loop stopped";
}

void WmiWorkerThread::cleanupWmi() {
    if (m_pSvc) {
        m_pSvc->CancelAsyncCall(m_pSink);
    }

    if (m_pSink) {
        m_pSink->Release();
        m_pSink = nullptr;
    }

    if (m_pStubUnk) {
        m_pStubUnk->Release();
        m_pStubUnk = nullptr;
    }

    if (m_pUnsecApp) {
        m_pUnsecApp->Release();
        m_pUnsecApp = nullptr;
    }

    if (m_pSvc) {
        m_pSvc->Release();
        m_pSvc = nullptr;
    }

    if (m_pLoc) {
        m_pLoc->Release();
        m_pLoc = nullptr;
    }
}

// WmiEventSink Implementation
WmiEventSink::WmiEventSink(WmiWorkerThread* parent) : m_refCount(1), m_parent(parent) {}

WmiEventSink::~WmiEventSink() {}

HRESULT WmiEventSink::QueryInterface(REFIID riid, void** ppv) {
    if (riid == IID_IUnknown || riid == IID_IWbemObjectSink) {
        *ppv = static_cast<IWbemObjectSink*>(this);
        AddRef();
        return WBEM_S_NO_ERROR;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

ULONG WmiEventSink::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

ULONG WmiEventSink::Release() {
    LONG refCount = InterlockedDecrement(&m_refCount);
    if (refCount == 0) {
        delete this;
    }
    return refCount;
}

HRESULT WmiEventSink::Indicate(LONG lObjectCount, IWbemClassObject** apObjArray) {
    for (LONG i = 0; i < lObjectCount; i++) {
        DWORD processId;
        QString processName;
        QString executablePath;

        if (extractProcessInfo(apObjArray[i], &processId, &processName, &executablePath)) {
            // Use QMetaObject::invokeMethod to safely emit signal from worker thread
            QMetaObject::invokeMethod(
                m_parent,
                "processCreated",
                Qt::QueuedConnection,
                Q_ARG(DWORD, processId),
                Q_ARG(QString, processName),
                Q_ARG(QString, executablePath)
            );
        }
    }

    return WBEM_S_NO_ERROR;
}

HRESULT WmiEventSink::SetStatus(LONG lFlags, HRESULT hResult, BSTR strParam, IWbemClassObject* pObjParam) {
    if (FAILED(hResult) && hResult != WBEM_E_CALL_CANCELLED) {
        QString errorMsg = QString("WMI SetStatus error: 0x%1").arg(static_cast<uint32_t>(hResult), 8, 16, QChar('0'));
        QMetaObject::invokeMethod(m_parent, "wmiError", Qt::QueuedConnection, Q_ARG(QString, errorMsg));
    }

    return WBEM_S_NO_ERROR;
}

bool WmiEventSink::extractProcessInfo(
    IWbemClassObject* pObj,
    DWORD* processId,
    QString* processName,
    QString* executablePath
) {
    if (!pObj || !processId || !processName || !executablePath) {
        return false;
    }

    VARIANT vtInstance;
    HRESULT hr = pObj->Get(L"TargetInstance", 0, &vtInstance, nullptr, nullptr);
    if (FAILED(hr) || vtInstance.vt != VT_UNKNOWN) {
        return false;
    }

    IWbemClassObject* pTargetInstance = nullptr;
    hr = vtInstance.punkVal->QueryInterface(IID_IWbemClassObject, reinterpret_cast<void**>(&pTargetInstance));
    VariantClear(&vtInstance);

    if (FAILED(hr)) {
        return false;
    }

    VARIANT vtProp;

    // Get process ID from the target instance
    hr = pTargetInstance->Get(L"ProcessId", 0, &vtProp, nullptr, nullptr);
    if (FAILED(hr) || (vtProp.vt != VT_I4 && vtProp.vt != VT_UI4)) {
        pTargetInstance->Release();
        return false;
    }
    *processId = vtProp.ulVal;
    VariantClear(&vtProp);

    // Get process name from the target instance
    hr = pTargetInstance->Get(L"Name", 0, &vtProp, nullptr, nullptr);
    if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
        *processName = bstrToQString(vtProp.bstrVal);
        VariantClear(&vtProp);
    } else {
        *processName = QString("Unknown");
    }

    pTargetInstance->Release();

    // Get executable path using ProcUtils (more reliable than WMI for path)
    *executablePath = ProcUtils::getProcessExecutablePath(*processId);
    if (executablePath->isEmpty()) {
        // Fallback: use process name as path
        *executablePath = *processName;
    }

    return true;
}

QString WmiEventSink::bstrToQString(BSTR bstr) {
    if (!bstr) {
        return QString();
    }
    return QString::fromWCharArray(bstr, SysStringLen(bstr));
}
