// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QFileSystemWatcher>
#include <QHash>
#include <QJsonArray>
#include <QLocalSocket>
#include <QProcess>
#include <QSet>
#include <QTimer>
#include <QVariant>

class DockModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int dockWidth READ dockWidth NOTIFY dockWidthChanged)
    Q_PROPERTY(int dockHeight READ dockHeight NOTIFY dockHeightChanged)
    Q_PROPERTY(int iconSize READ iconSize NOTIFY iconSizeChanged)
    Q_PROPERTY(int edgeMargin READ edgeMargin NOTIFY edgeMarginChanged)
    Q_PROPERTY(QString screenPlacement READ screenPlacement NOTIFY screenPlacementChanged)
    Q_PROPERTY(QString configFile READ configFile CONSTANT)
    Q_PROPERTY(bool autoHide READ autoHide NOTIFY autoHideChanged)
    Q_PROPERTY(int autoHideDelay READ autoHideDelay NOTIFY autoHideDelayChanged)
    Q_PROPERTY(bool showAboveFullscreen READ showAboveFullscreen NOTIFY showAboveFullscreenChanged)
    Q_PROPERTY(bool fullscreenActive READ fullscreenActive NOTIFY fullscreenActiveChanged)
    Q_PROPERTY(bool compositorAvailable READ compositorAvailable NOTIFY compositorAvailableChanged)

public:
    enum Role
    {
        AppIdRole = Qt::UserRole + 1,
        NameRole,
        IconRole,
        RunningRole,
        ActiveRole,
        PinnedRole,
        WindowCountRole,
        LaunchableRole,
        ActiveWindowIndexRole,
        MessageCountRole,
        LaunchingRole
    };
    Q_ENUM(Role)

    explicit DockModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int dockWidth() const;
    int dockHeight() const;
    int iconSize() const;
    int edgeMargin() const;
    QString screenPlacement() const;
    QString configFile() const;
    bool autoHide() const;
    int autoHideDelay() const;
    bool showAboveFullscreen() const;
    bool fullscreenActive() const;
    bool fullscreenActiveOnScreen(const QString &screenName) const;
    bool compositorAvailable() const;

    Q_INVOKABLE void trigger(int row);
    Q_INVOKABLE void launchNew(int row);
    Q_INVOKABLE void closeWindows(int row);
    Q_INVOKABLE void togglePinned(int row);
    Q_INVOKABLE void movePinned(int fromRow, int toRow);
    Q_INVOKABLE void refresh();

signals:
    void dockWidthChanged();
    void dockHeightChanged();
    void iconSizeChanged();
    void edgeMarginChanged();
    void screenPlacementChanged();
    void autoHideChanged();
    void autoHideDelayChanged();
    void showAboveFullscreenChanged();
    void compositorAvailableChanged();
    void fullscreenActiveChanged();
    void launchFailed(const QString &applicationName);

private slots:
    void updateLauncherEntry(const QString &applicationUri,
                             const QVariantMap &properties);

private:
    struct DesktopEntry
    {
        QString id;
        QString name;
        QString icon;
        QString executable;
        QString startupWmClass;
        bool terminal = false;
    };

    struct DockEntry
    {
        struct Window
        {
            QString address;
            bool active = false;
        };

        QString appId;
        QString name;
        QString icon;
        QString executable;
        QList<Window> windows;
        bool terminal = false;
        bool active = false;
        bool pinned = false;
    };

    void initializeSettings();
    void reloadSettings();
    void scheduleSettingsReload();
    void discoverDesktopEntries();
    void registerDesktopAlias(const QString &alias, const QString &desktopId);
    QString desktopIdForWindowClass(const QString &windowClass) const;
    QString desktopIdForLauncherUri(const QString &applicationUri) const;
    void activateWindow(const QString &address);
    void rebuild(const QJsonArray &clients);
    void applyClientReply(int exitCode, QProcess::ExitStatus exitStatus);
    void applyMonitorReply(int exitCode, QProcess::ExitStatus exitStatus);
    void applyActiveWindowReply(int exitCode, QProcess::ExitStatus exitStatus);
    void rebuildWhenReady();
    void savePinnedIds();
    void connectEventSocket();
    void scheduleEventSocketReconnect();
    void readEventSocket();
    void handleEventLine(const QByteArray &line);
    void collectProcessOutput(QProcess *process, QByteArray *output);
    bool launch(const DockEntry &entry);
    void setLaunching(const QString &appId, bool launching);
    static QString validatedIconSource(const QString &value);
    static QString normalizedId(const QString &value);
    static QString executableFromExec(const QString &exec);
    static QStringList commandFromExec(const QString &exec);
    static QString displayNameForClass(const QString &windowClass);

    QHash<QString, DesktopEntry> m_desktopEntries;
    QHash<QString, QString> m_desktopAliases;
    QList<DockEntry> m_entries;
    QStringList m_pinnedIds;
    QString m_configFile;
    int m_dockWidth = 0;
    int m_dockHeight = 72;
    int m_iconSize = 48;
    int m_edgeMargin = 8;
    QString m_screenPlacement = QStringLiteral("all");
    bool m_autoHide = false;
    int m_autoHideDelay = 650;
    bool m_showAboveFullscreen = false;
    bool m_fullscreenActive = false;
    bool m_compositorAvailable = false;
    bool m_clientsReady = false;
    bool m_monitorsReady = false;
    bool m_activeWindowReady = false;
    bool m_refreshPending = false;
    QJsonArray m_lastClients;
    QJsonArray m_pendingClients;
    QString m_activeWindowAddress;
    QString m_pendingActiveWindowAddress;
    QSet<int> m_fullscreenMonitorIds;
    QSet<QString> m_closedWindowAddresses;
    QSet<QString> m_unfocusableWindowAddresses;
    QHash<int, QString> m_monitorNames;
    QHash<QString, int> m_cycleIndices;
    QHash<QString, qint64> m_launcherEntryCounts;
    QHash<QString, bool> m_launcherEntryCountVisible;
    QHash<QString, int> m_messageCounts;
    QSet<QString> m_launchingAppIds;
    QHash<QString, QTimer *> m_launchTimers;
    QString m_hyprctlProgram;
    QString m_terminalProgram;
    QProcess m_clientsProcess;
    QProcess m_monitorsProcess;
    QProcess m_activeWindowProcess;
    QByteArray m_clientsOutput;
    QByteArray m_monitorsOutput;
    QByteArray m_activeWindowOutput;
    QTimer m_clientsTimeoutTimer;
    QTimer m_monitorsTimeoutTimer;
    QTimer m_activeWindowTimeoutTimer;
    QLocalSocket m_eventSocket;
    QByteArray m_eventBuffer;
    QTimer m_eventRefreshTimer;
    QTimer m_eventReconnectTimer;
    QTimer m_refreshTimer;
    QFileSystemWatcher m_settingsWatcher;
    QTimer m_settingsReloadTimer;
};
