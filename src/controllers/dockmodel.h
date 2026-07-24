// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QJsonArray>
#include <QLocalSocket>
#include <QProcess>
#include <QSet>
#include <QTimer>

class DockModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int dockWidth READ dockWidth CONSTANT)
    Q_PROPERTY(int dockHeight READ dockHeight CONSTANT)
    Q_PROPERTY(int iconSize READ iconSize CONSTANT)
    Q_PROPERTY(int edgeMargin READ edgeMargin CONSTANT)
    Q_PROPERTY(QString screenPlacement READ screenPlacement CONSTANT)
    Q_PROPERTY(QString configFile READ configFile CONSTANT)
    Q_PROPERTY(bool autoHide READ autoHide CONSTANT)
    Q_PROPERTY(int autoHideDelay READ autoHideDelay CONSTANT)
    Q_PROPERTY(bool showAboveFullscreen READ showAboveFullscreen CONSTANT)
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
        ActiveWindowIndexRole
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
    Q_INVOKABLE void togglePinned(int row);
    Q_INVOKABLE void movePinned(int fromRow, int toRow);
    Q_INVOKABLE void refresh();

signals:
    void compositorAvailableChanged();
    void fullscreenActiveChanged();
    void launchFailed(const QString &applicationName);

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
    void discoverDesktopEntries();
    void registerDesktopAlias(const QString &alias, const QString &desktopId);
    QString desktopIdForWindowClass(const QString &windowClass) const;
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
    bool launch(const DockEntry &entry);
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
    QSet<QString> m_unfocusableWindowAddresses;
    QHash<int, QString> m_monitorNames;
    QHash<QString, int> m_cycleIndices;
    QProcess m_clientsProcess;
    QProcess m_monitorsProcess;
    QProcess m_activeWindowProcess;
    QLocalSocket m_eventSocket;
    QByteArray m_eventBuffer;
    QTimer m_eventRefreshTimer;
    QTimer m_eventReconnectTimer;
    QTimer m_refreshTimer;
};
