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

    bool hideTaskbarIcon() const;
    void setHideTaskbarIcon(bool enabled);

    // Force save to disk
    void sync();

    // Reset all settings to defaults
    void resetToDefaults();

   signals:
    void autoRefreshChanged(bool enabled);
    void refreshIntervalChanged(double interval);
    void hideFromScreenCaptureChanged(bool enabled);
    void randomizeWindowTitlesChanged(bool enabled);
    void hideTaskbarIconChanged(bool enabled);

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
    static constexpr bool kDefaultHideTaskbarIcon = false;
};
