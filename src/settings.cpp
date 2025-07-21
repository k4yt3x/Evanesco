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

void Settings::resetToDefaults() {
    setAutoRefresh(kDefaultAutoRefresh);
    setRefreshInterval(kDefaultRefreshInterval);
    setHideFromScreenCapture(kDefaultHideFromScreenCapture);
    setRandomizeWindowTitles(kDefaultRandomizeWindowTitles);
    setHideTaskbarIcon(kDefaultHideTaskbarIcon);
    setRandomizeDllFileName(kDefaultRandomizeDllFileName);
    setHideTargetTaskbarIcons(kDefaultHideTargetTaskbarIcons);
    sync();
}
