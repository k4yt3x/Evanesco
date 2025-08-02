#pragma once

#include <QObject>
#include <QSettings>
#include <QString>

class Settings : public QObject {
    Q_OBJECT

   public:
    static Settings* instance();

    // General settings
    bool autoRefresh() const;
    void setAutoRefresh(bool enabled);

    double refreshInterval() const;
    void setRefreshInterval(double interval);

    bool hideFromScreenCapture() const;
    void setHideFromScreenCapture(bool enabled);

    bool randomizeWindowTitles() const;
    void setRandomizeWindowTitles(bool enabled);

    bool randomizeTrayIcon() const;
    void setRandomizeTrayIcon(bool enabled);

    bool minimizeToTray() const;
    void setMinimizeToTray(bool enabled);

    bool hideTaskbarIcon() const;
    void setHideTaskbarIcon(bool enabled);

    // Payload settings
    bool randomizeDllFileName() const;
    void setRandomizeDllFileName(bool enabled);

    bool hideTargetTaskbarIcons() const;
    void setHideTargetTaskbarIcons(bool enabled);

    // Autohide settings
    bool autohideEnabled() const;
    void setAutohideEnabled(bool enabled);

    bool autohideNotify() const;
    void setAutohideNotify(bool enabled);

    int maxWindowCreationWaitMs() const;
    void setMaxWindowCreationWaitMs(int waitMs);

    QStringList autohideList() const;
    void setAutohideList(const QStringList& list);

    // Force save to disk
    void sync();

    // Reset all settings to defaults
    void resetToDefaults();

   signals:
    void autoRefreshChanged(bool enabled);
    void refreshIntervalChanged(double interval);
    void hideFromScreenCaptureChanged(bool enabled);
    void randomizeWindowTitlesChanged(bool enabled);
    void randomizeTrayIconChanged(bool enabled);
    void minimizeToTrayChanged(bool enabled);
    void maxWindowCreationWaitMsChanged(int waitMs);
    void hideTaskbarIconChanged(bool enabled);
    void randomizeDllFileNameChanged(bool enabled);
    void hideTargetTaskbarIconsChanged(bool enabled);
    void autohideEnabledChanged(bool enabled);
    void autohideNotifyChanged(bool enabled);
    void autohideListChanged(const QStringList& list);

   private:
    explicit Settings(QObject* parent = nullptr);
    ~Settings();

    static Settings* m_instance;
    QSettings* m_settings;

    // Default values
    static constexpr bool kDefaultAutoRefresh = false;
    static constexpr double kDefaultRefreshInterval = 1.0;
    static constexpr bool kDefaultHideFromScreenCapture = false;
    static constexpr bool kDefaultRandomizeWindowTitles = false;
    static constexpr bool kDefaultRandomizeTrayIcon = false;
    static constexpr bool kDefaultMinimizeToTray = true;
    static constexpr int kDefaultMaxWindowCreationWaitMs = 3000;
    static constexpr bool kDefaultHideTaskbarIcon = false;
    static constexpr bool kDefaultRandomizeDllFileName = false;
    static constexpr bool kDefaultHideTargetTaskbarIcons = false;
    static constexpr bool kDefaultAutohideEnabled = false;
    static constexpr bool kDefaultAutohideNotify = true;
};
