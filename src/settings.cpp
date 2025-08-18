#include "settings.h"

#include <QApplication>

Settings* Settings::m_instance = nullptr;

Settings* Settings::instance() {
    if (!m_instance) {
        m_instance = new Settings();
    }
    return m_instance;
}

Settings::Settings(QObject* parent) : QObject(parent) {
    QString configPath = QApplication::applicationDirPath() + "/Evanesco.ini";
    m_settings = new QSettings(configPath, QSettings::IniFormat);
}

Settings::~Settings() {
    delete m_settings;
}

bool Settings::autoRefresh() const {
    return m_settings->value("general/autoRefresh", kDefaultAutoRefresh).toBool();
}

void Settings::setAutoRefresh(bool enabled) {
    if (autoRefresh() != enabled) {
        m_settings->setValue("general/autoRefresh", enabled);
        emit autoRefreshChanged(enabled);
    }
}

double Settings::refreshInterval() const {
    return m_settings->value("general/refreshInterval", kDefaultRefreshInterval).toDouble();
}

void Settings::setRefreshInterval(double interval) {
    if (refreshInterval() != interval) {
        m_settings->setValue("general/refreshInterval", interval);
        emit refreshIntervalChanged(interval);
    }
}

bool Settings::hideFromScreenCapture() const {
    return m_settings->value("general/hideFromScreenCapture", kDefaultHideFromScreenCapture).toBool();
}

void Settings::setHideFromScreenCapture(bool enabled) {
    if (hideFromScreenCapture() != enabled) {
        m_settings->setValue("general/hideFromScreenCapture", enabled);
        emit hideFromScreenCaptureChanged(enabled);
    }
}

bool Settings::randomizeWindowTitles() const {
    return m_settings->value("general/randomizeWindowTitles", kDefaultRandomizeWindowTitles).toBool();
}

void Settings::setRandomizeWindowTitles(bool enabled) {
    if (randomizeWindowTitles() != enabled) {
        m_settings->setValue("general/randomizeWindowTitles", enabled);
        emit randomizeWindowTitlesChanged(enabled);
    }
}

bool Settings::randomizeTrayIcon() const {
    return m_settings->value("general/randomizeTrayIcon", kDefaultRandomizeTrayIcon).toBool();
}

void Settings::setRandomizeTrayIcon(bool enabled) {
    if (randomizeTrayIcon() != enabled) {
        m_settings->setValue("general/randomizeTrayIcon", enabled);
        emit randomizeTrayIconChanged(enabled);
    }
}

bool Settings::enableTrayIcon() const {
    return m_settings->value("general/enableTrayIcon", kDefaultEnableTrayIcon).toBool();
}

void Settings::setEnableTrayIcon(bool enabled) {
    if (enableTrayIcon() != enabled) {
        m_settings->setValue("general/enableTrayIcon", enabled);
        emit enableTrayIconChanged(enabled);
    }
}

bool Settings::closeToTray() const {
    return m_settings->value("general/closeToTray", kDefaultCloseToTray).toBool();
}

void Settings::setCloseToTray(bool enabled) {
    if (closeToTray() != enabled) {
        m_settings->setValue("general/closeToTray", enabled);
        emit closeToTrayChanged(enabled);
    }
}

void Settings::sync() {
    m_settings->sync();
}

bool Settings::hideTaskbarIcon() const {
    return m_settings->value("general/hideTaskbarIcon", kDefaultHideTaskbarIcon).toBool();
}

void Settings::setHideTaskbarIcon(bool enabled) {
    if (hideTaskbarIcon() != enabled) {
        m_settings->setValue("general/hideTaskbarIcon", enabled);
        emit hideTaskbarIconChanged(enabled);
    }
}

bool Settings::randomizeDllFileName() const {
    return m_settings->value("payload/randomizeDllFileName", kDefaultRandomizeDllFileName).toBool();
}

void Settings::setRandomizeDllFileName(bool enabled) {
    if (randomizeDllFileName() != enabled) {
        m_settings->setValue("payload/randomizeDllFileName", enabled);
        emit randomizeDllFileNameChanged(enabled);
    }
}

bool Settings::hideTargetTaskbarIcons() const {
    return m_settings->value("payload/hideTargetTaskbarIcons", kDefaultHideTargetTaskbarIcons).toBool();
}

void Settings::setHideTargetTaskbarIcons(bool enabled) {
    if (hideTargetTaskbarIcons() != enabled) {
        m_settings->setValue("payload/hideTargetTaskbarIcons", enabled);
        emit hideTargetTaskbarIconsChanged(enabled);
    }
}

bool Settings::autohideEnabled() const {
    return m_settings->value("autohide/enabled", kDefaultAutohideEnabled).toBool();
}

void Settings::setAutohideEnabled(bool enabled) {
    if (autohideEnabled() != enabled) {
        m_settings->setValue("autohide/enabled", enabled);
        emit autohideEnabledChanged(enabled);
    }
}

bool Settings::autohideNotify() const {
    return m_settings->value("autohide/notify", kDefaultAutohideNotify).toBool();
}

void Settings::setAutohideNotify(bool enabled) {
    if (autohideNotify() != enabled) {
        m_settings->setValue("autohide/notify", enabled);
        emit autohideNotifyChanged(enabled);
    }
}

int Settings::maxWindowCreationWaitMs() const {
    return m_settings->value("autohide/maxWindowCreationWaitMs", kDefaultMaxWindowCreationWaitMs).toInt();
}

void Settings::setMaxWindowCreationWaitMs(int waitMs) {
    if (maxWindowCreationWaitMs() != waitMs) {
        m_settings->setValue("autohide/maxWindowCreationWaitMs", waitMs);
        emit maxWindowCreationWaitMsChanged(waitMs);
    }
}

bool Settings::hideAutohideProcessesOnStart() const {
    return m_settings->value("autohide/hideAutohideProcessesOnStart", kDefaultHideAutohideProcessesOnStart).toBool();
}

void Settings::setHideAutohideProcessesOnStart(bool enabled) {
    if (hideAutohideProcessesOnStart() != enabled) {
        m_settings->setValue("autohide/hideAutohideProcessesOnStart", enabled);
        emit hideAutohideProcessesOnStartChanged(enabled);
    }
}

QStringList Settings::autohideList() const {
    return m_settings->value("autohide/autohideList", QStringList()).toStringList();
}

void Settings::setAutohideList(const QStringList& list) {
    if (autohideList() != list) {
        m_settings->setValue("autohide/autohideList", list);
        emit autohideListChanged(list);
    }
}

void Settings::resetToDefaults() {
    setAutoRefresh(kDefaultAutoRefresh);
    setRefreshInterval(kDefaultRefreshInterval);
    setHideFromScreenCapture(kDefaultHideFromScreenCapture);
    setRandomizeWindowTitles(kDefaultRandomizeWindowTitles);
    setRandomizeTrayIcon(kDefaultRandomizeTrayIcon);
    setEnableTrayIcon(kDefaultEnableTrayIcon);
    setCloseToTray(kDefaultCloseToTray);
    setMaxWindowCreationWaitMs(kDefaultMaxWindowCreationWaitMs);
    setHideTaskbarIcon(kDefaultHideTaskbarIcon);
    setRandomizeDllFileName(kDefaultRandomizeDllFileName);
    setHideTargetTaskbarIcons(kDefaultHideTargetTaskbarIcons);
    setAutohideEnabled(kDefaultAutohideEnabled);
    setAutohideNotify(kDefaultAutohideNotify);
    setHideAutohideProcessesOnStart(kDefaultHideAutohideProcessesOnStart);
    setAutohideList(QStringList());
    sync();
}
