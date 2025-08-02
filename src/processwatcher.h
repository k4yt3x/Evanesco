#pragma once

#include <comdef.h>
#include <wbemidl.h>
#include <windows.h>

#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThread>

// Forward declarations
class WmiEventSink;
class WmiWorkerThread;

class ProcessWatcher : public QObject {
    Q_OBJECT

   public:
    explicit ProcessWatcher(QObject* parent = nullptr);
    ~ProcessWatcher();

    // Control methods
    void start();
    void stop();
    bool isRunning() const;

    // Configuration methods
    void setList(const QStringList& list);
    QStringList getList() const;

   signals:
    void processHidden(DWORD processId, const QString& processName, const QString& executablePath);
    void processDetected(DWORD processId, const QString& processName, const QString& executablePath);
    void errorOccurred(const QString& errorMessage);
    void notificationRequested(const QString& title, const QString& message);

   private slots:
    void onProcessCreated(DWORD processId, const QString& processName, const QString& executablePath);
    void onWmiError(const QString& errorMessage);

   private:
    // Core functionality
    bool shouldProcessBeHidden(const QString& executablePath) const;
    bool processHasWindows(DWORD processId) const;
    bool hideProcessWindows(DWORD processId, QString* errorMessage = nullptr);
    QString getProcessName(const QString& executablePath) const;

    // Utility methods
    bool isProcessNameInList(const QString& processName) const;
    bool isFullPathInList(const QString& fullPath) const;
    QString normalizePath(const QString& path) const;

    // Member variables
    WmiWorkerThread* m_workerThread;
    QStringList m_list;
    bool m_isRunning;
    mutable QMutex m_mutex;

    // Constants for window detection retry logic
    static constexpr int kWindowCheckIntervalMs = 100;

    friend class WmiEventSink;
    friend class WmiWorkerThread;
};

// Worker thread for WMI operations
class WmiWorkerThread : public QThread {
    Q_OBJECT

   public:
    explicit WmiWorkerThread(QObject* parent = nullptr);
    ~WmiWorkerThread();

    void startWatching();
    void stopWatching();

   signals:
    void processCreated(DWORD processId, const QString& processName, const QString& executablePath);
    void wmiError(const QString& errorMessage);

   protected:
    void run() override;

   private:
    bool initializeWmi();
    void cleanupWmi();
    bool setupEventSink();
    void processWmiEvents();

    // WMI COM objects
    IWbemServices* m_pSvc;
    IWbemLocator* m_pLoc;
    IUnsecuredApartment* m_pUnsecApp;
    IUnknown* m_pStubUnk;
    WmiEventSink* m_pSink;

    // Thread control
    volatile bool m_shouldStop;
    HANDLE m_stopEvent;
};

// WMI event sink for process creation events
class WmiEventSink : public IWbemObjectSink {
   public:
    explicit WmiEventSink(WmiWorkerThread* parent);
    virtual ~WmiEventSink();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IWbemObjectSink methods
    STDMETHODIMP Indicate(LONG lObjectCount, IWbemClassObject** apObjArray) override;
    STDMETHODIMP SetStatus(LONG lFlags, HRESULT hResult, BSTR strParam, IWbemClassObject* pObjParam) override;

   private:
    LONG m_refCount;
    WmiWorkerThread* m_parent;

    // Helper methods
    bool extractProcessInfo(IWbemClassObject* pObj, DWORD* processId, QString* processName, QString* executablePath);
    QString bstrToQString(BSTR bstr);
};
