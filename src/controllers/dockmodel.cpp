// SPDX-License-Identifier: BSD-3-Clause

#include "dockmodel.h"

#include <algorithm>
#include <array>
#include <ranges>

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>

namespace
{
constexpr auto kDefaultPinnedApplications = std::to_array<const char *>({
    "org.kde.index",
    "org.kde.fiery",
    "org.kde.vvave",
    "org.kde.pix",
    "org.kde.nota",
    "org.kde.station"
});

QString hyprlandEventSocketPath()
{
    const QString signature =
        QString::fromLocal8Bit(qgetenv("HYPRLAND_INSTANCE_SIGNATURE")).trimmed();
    if (signature.isEmpty())
        return {};

    QString runtimeDirectory =
        QString::fromLocal8Bit(qgetenv("XDG_RUNTIME_DIR")).trimmed();
    if (runtimeDirectory.isEmpty())
        runtimeDirectory = QStringLiteral("/tmp");

    return QStringLiteral("%1/hypr/%2/.socket2.sock")
        .arg(runtimeDirectory, signature);
}
}

DockModel::DockModel(QObject *parent)
    : QAbstractListModel(parent)
{
    initializeSettings();
    discoverDesktopEntries();

    connect(&m_clientsProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &DockModel::applyClientReply);
    connect(&m_monitorsProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &DockModel::applyMonitorReply);
    connect(&m_activeWindowProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &DockModel::applyActiveWindowReply);

    m_eventRefreshTimer.setSingleShot(true);
    m_eventRefreshTimer.setInterval(40);
    m_eventRefreshTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_eventRefreshTimer, &QTimer::timeout, this, &DockModel::refresh);

    m_eventReconnectTimer.setSingleShot(true);
    m_eventReconnectTimer.setInterval(2000);
    connect(&m_eventReconnectTimer,
            &QTimer::timeout,
            this,
            &DockModel::connectEventSocket);

    connect(&m_eventSocket, &QLocalSocket::readyRead, this, &DockModel::readEventSocket);
    connect(&m_eventSocket, &QLocalSocket::connected, &m_eventReconnectTimer, &QTimer::stop);
    connect(&m_eventSocket, &QLocalSocket::disconnected, this, [this]() {
        m_eventBuffer.clear();
        scheduleEventSocketReconnect();
    });
    connect(&m_eventSocket,
            &QLocalSocket::errorOccurred,
            this,
            [this](QLocalSocket::LocalSocketError) {
                scheduleEventSocketReconnect();
            });

    m_refreshTimer.setInterval(10000);
    m_refreshTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_refreshTimer, &QTimer::timeout, this, &DockModel::refresh);
    m_refreshTimer.start();

    rebuild({});
    refresh();
    connectEventSocket();
}

int DockModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant DockModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const DockEntry &entry = m_entries.at(index.row());
    switch (role)
    {
        case AppIdRole:
            return entry.appId;
        case NameRole:
            return entry.name;
        case IconRole:
            return entry.icon;
        case RunningRole:
            return !entry.windows.isEmpty();
        case ActiveRole:
            return entry.active;
        case PinnedRole:
            return entry.pinned;
        case WindowCountRole:
            return entry.windows.size();
        case LaunchableRole:
            return !entry.executable.isEmpty();
        case ActiveWindowIndexRole:
            for (qsizetype windowIndex = 0; windowIndex < entry.windows.size(); ++windowIndex)
            {
                if (entry.windows.at(windowIndex).active)
                    return static_cast<int>(windowIndex);
            }
            return -1;
        default:
            return {};
    }
}

QHash<int, QByteArray> DockModel::roleNames() const
{
    return {
        {AppIdRole, "appId"},
        {NameRole, "name"},
        {IconRole, "iconName"},
        {RunningRole, "running"},
        {ActiveRole, "active"},
        {PinnedRole, "pinned"},
        {WindowCountRole, "windowCount"},
        {LaunchableRole, "launchable"},
        {ActiveWindowIndexRole, "activeWindowIndex"}
    };
}

int DockModel::dockWidth() const
{
    return m_dockWidth;
}

int DockModel::dockHeight() const
{
    return m_dockHeight;
}

int DockModel::iconSize() const
{
    return m_iconSize;
}

int DockModel::edgeMargin() const
{
    return m_edgeMargin;
}

QString DockModel::screenPlacement() const
{
    return m_screenPlacement;
}

QString DockModel::configFile() const
{
    return m_configFile;
}

bool DockModel::autoHide() const
{
    return m_autoHide;
}

int DockModel::autoHideDelay() const
{
    return m_autoHideDelay;
}

bool DockModel::showAboveFullscreen() const
{
    return m_showAboveFullscreen;
}

bool DockModel::fullscreenActive() const
{
    return m_fullscreenActive;
}

bool DockModel::fullscreenActiveOnScreen(const QString &screenName) const
{
    if (!m_fullscreenActive)
        return false;

    const QString trimmedName = screenName.trimmed();
    if (trimmedName.isEmpty() || m_monitorNames.isEmpty())
        return true;

    for (int monitorId : m_fullscreenMonitorIds)
    {
        if (m_monitorNames.value(monitorId) == trimmedName)
            return true;
    }
    return false;
}

bool DockModel::compositorAvailable() const
{
    return m_compositorAvailable;
}

void DockModel::trigger(int row)
{
    if (row < 0 || row >= m_entries.size())
        return;

    const DockEntry entry = m_entries.at(row);
    if (!entry.windows.isEmpty())
    {
        int windowIndex = 0;
        if (entry.windows.size() > 1)
        {
            const int storedIndex = m_cycleIndices.value(entry.appId, -1);
            if (storedIndex >= 0)
            {
                windowIndex = storedIndex % entry.windows.size();
            }
            else
            {
                for (qsizetype index = 0; index < entry.windows.size(); ++index)
                {
                    if (entry.windows.at(index).active)
                    {
                        windowIndex = (index + 1) % entry.windows.size();
                        break;
                    }
                }
            }
            m_cycleIndices.insert(entry.appId, (windowIndex + 1) % entry.windows.size());
        }
        activateWindow(entry.windows.at(windowIndex).address);
        return;
    }

    if (!launch(entry))
        emit launchFailed(entry.name);
}

void DockModel::launchNew(int row)
{
    if (row < 0 || row >= m_entries.size())
        return;

    const DockEntry entry = m_entries.at(row);
    if (!launch(entry))
        emit launchFailed(entry.name);
}

void DockModel::togglePinned(int row)
{
    if (row < 0 || row >= m_entries.size())
        return;

    const QString appId = m_entries.at(row).appId;
    const int pinnedIndex = m_pinnedIds.indexOf(appId);
    if (pinnedIndex >= 0)
        m_pinnedIds.removeAt(pinnedIndex);
    else
        m_pinnedIds.append(appId);

    savePinnedIds();
    rebuild(m_lastClients);
    refresh();
}

void DockModel::movePinned(int fromRow, int toRow)
{
    if (fromRow < 0 || fromRow >= m_entries.size() || !m_entries.at(fromRow).pinned)
        return;

    int pinnedCount = 0;
    while (pinnedCount < m_entries.size() && m_entries.at(pinnedCount).pinned)
        ++pinnedCount;
    if (pinnedCount < 2)
        return;

    toRow = qBound(0, toRow, pinnedCount - 1);
    const QString sourceId = m_entries.at(fromRow).appId;
    const QString targetId = m_entries.at(toRow).appId;
    const int sourceIndex = m_pinnedIds.indexOf(sourceId);
    const int targetIndex = m_pinnedIds.indexOf(targetId);
    if (sourceIndex < 0 || targetIndex < 0 || sourceIndex == targetIndex)
        return;

    m_pinnedIds.move(sourceIndex, targetIndex);
    savePinnedIds();
    rebuild(m_lastClients);
}

void DockModel::activateWindow(const QString &address)
{
    const QString trimmedAddress = address.trimmed();
    static const QRegularExpression addressPattern(QStringLiteral("^0x[0-9a-fA-F]+$"));
    if (!addressPattern.match(trimmedAddress).hasMatch())
        return;

    auto *focusProcess = new QProcess(this);
    focusProcess->setProgram(QStringLiteral("hyprctl"));
    focusProcess->setArguments(
        {QStringLiteral("dispatch"),
         QStringLiteral("hl.dsp.focus({ window = 'address:%1' })")
             .arg(trimmedAddress)});
    focusProcess->setProcessChannelMode(QProcess::SeparateChannels);

    const auto finishFocusRequest = [this, focusProcess, trimmedAddress]() {
        if (focusProcess->property("marinaFocusRequestFinished").toBool())
            return;

        focusProcess->setProperty("marinaFocusRequestFinished", true);
        const QByteArray output = focusProcess->readAllStandardOutput()
            + focusProcess->readAllStandardError();
        if (output.toLower().contains(QByteArrayLiteral("window not found")))
        {
            m_unfocusableWindowAddresses.insert(trimmedAddress);
            rebuild(m_lastClients);
        }
        focusProcess->deleteLater();
        refresh();
    };
    connect(focusProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            focusProcess,
            [finishFocusRequest](int, QProcess::ExitStatus) {
                finishFocusRequest();
            });
    connect(focusProcess,
            &QProcess::errorOccurred,
            focusProcess,
            [focusProcess, finishFocusRequest](QProcess::ProcessError) {
                if (focusProcess->state() == QProcess::NotRunning)
                    finishFocusRequest();
            });
    focusProcess->start(QIODevice::ReadOnly);
}

void DockModel::refresh()
{
    if (m_clientsProcess.state() != QProcess::NotRunning
        || m_monitorsProcess.state() != QProcess::NotRunning
        || m_activeWindowProcess.state() != QProcess::NotRunning)
    {
        m_refreshPending = true;
        return;
    }

    m_refreshPending = false;
    m_clientsReady = false;
    m_monitorsReady = false;
    m_activeWindowReady = false;
    m_monitorsProcess.setProgram(QStringLiteral("hyprctl"));
    m_monitorsProcess.setArguments({QStringLiteral("-j"), QStringLiteral("monitors")});
    m_monitorsProcess.start(QIODevice::ReadOnly);

    m_activeWindowProcess.setProgram(QStringLiteral("hyprctl"));
    m_activeWindowProcess.setArguments(
        {QStringLiteral("-j"), QStringLiteral("activewindow")});
    m_activeWindowProcess.start(QIODevice::ReadOnly);

    m_clientsProcess.setProgram(QStringLiteral("hyprctl"));
    // Hyprland may unmap clients on inactive workspaces. Keep them in the
    // global task list instead of silently reducing Marina to this workspace.
    m_clientsProcess.setArguments(
        {QStringLiteral("-j"), QStringLiteral("-a"), QStringLiteral("clients")});
    m_clientsProcess.start(QIODevice::ReadOnly);
}

void DockModel::initializeSettings()
{
    const QString configDirectory =
        QDir::home().filePath(QStringLiteral(".config/marina"));
    QDir().mkpath(configDirectory);
    m_configFile = configDirectory + QStringLiteral("/marina.conf");

    QSettings settings(m_configFile, QSettings::IniFormat);
    if (!settings.contains(QStringLiteral("Launchers/pinned")))
    {
        QStringList defaults;
        defaults.reserve(static_cast<qsizetype>(kDefaultPinnedApplications.size()));
        for (const char *desktopId : kDefaultPinnedApplications)
            defaults.append(QString::fromLatin1(desktopId));
        settings.setValue(QStringLiteral("Launchers/pinned"), defaults);
    }

    if (!settings.contains(QStringLiteral("Appearance/iconSize")))
        settings.setValue(QStringLiteral("Appearance/iconSize"), 48);
    if (!settings.contains(QStringLiteral("Appearance/edgeMargin")))
        settings.setValue(QStringLiteral("Appearance/edgeMargin"), 8);
    if (!settings.contains(QStringLiteral("Window/screenPlacement")))
        settings.setValue(QStringLiteral("Window/screenPlacement"), QStringLiteral("all"));
    if (!settings.contains(QStringLiteral("Window/width")))
        settings.setValue(QStringLiteral("Window/width"), 0);
    if (!settings.contains(QStringLiteral("Window/height")))
        settings.setValue(QStringLiteral("Window/height"), 0);
    if (!settings.contains(QStringLiteral("Window/showAboveFullscreen")))
        settings.setValue(QStringLiteral("Window/showAboveFullscreen"), false);
    if (!settings.contains(QStringLiteral("Behavior/autoHide")))
        settings.setValue(QStringLiteral("Behavior/autoHide"), false);
    if (!settings.contains(QStringLiteral("Behavior/autoHideDelay")))
        settings.setValue(QStringLiteral("Behavior/autoHideDelay"), 650);
    settings.remove(QStringLiteral("Behavior/currentWorkspaceOnly"));

    m_iconSize = qBound(32, settings.value(QStringLiteral("Appearance/iconSize")).toInt(), 96);
    m_edgeMargin = qBound(0, settings.value(QStringLiteral("Appearance/edgeMargin")).toInt(), 48);
    const int requestedWidth = settings.value(QStringLiteral("Window/width")).toInt();
    const int requestedHeight = settings.value(QStringLiteral("Window/height")).toInt();
    m_dockWidth = requestedWidth <= 0 ? 0 : qBound(96, requestedWidth, 4096);
    m_dockHeight = requestedHeight <= 0
        ? m_iconSize + 24
        : qBound(m_iconSize + 8, requestedHeight, 256);
    m_screenPlacement = settings.value(QStringLiteral("Window/screenPlacement"))
                            .toString().trimmed().toLower();
    if (m_screenPlacement != QLatin1String("active")
        && m_screenPlacement != QLatin1String("all"))
    {
        m_screenPlacement = QStringLiteral("all");
        settings.setValue(QStringLiteral("Window/screenPlacement"), m_screenPlacement);
    }

    m_showAboveFullscreen =
        settings.value(QStringLiteral("Window/showAboveFullscreen")).toBool();
    m_autoHide = settings.value(QStringLiteral("Behavior/autoHide")).toBool();
    m_autoHideDelay = qBound(
        0, settings.value(QStringLiteral("Behavior/autoHideDelay")).toInt(), 5000);
    m_pinnedIds = settings.value(QStringLiteral("Launchers/pinned")).toStringList();
    m_pinnedIds.removeDuplicates();
    settings.sync();
}

void DockModel::savePinnedIds()
{
    QSettings settings(m_configFile, QSettings::IniFormat);
    settings.setValue(QStringLiteral("Launchers/pinned"), m_pinnedIds);
    settings.sync();
}

void DockModel::connectEventSocket()
{
    const QString socketPath = hyprlandEventSocketPath();
    if (socketPath.isEmpty())
        return;

    if (m_eventSocket.state() == QLocalSocket::ConnectedState
        || m_eventSocket.state() == QLocalSocket::ConnectingState)
    {
        return;
    }

    m_eventBuffer.clear();
    m_eventSocket.abort();
    m_eventSocket.connectToServer(socketPath, QIODevice::ReadOnly);
}

void DockModel::scheduleEventSocketReconnect()
{
    if (!m_eventReconnectTimer.isActive())
        m_eventReconnectTimer.start();
}

void DockModel::readEventSocket()
{
    m_eventBuffer += m_eventSocket.readAll();

    int newlineIndex = m_eventBuffer.indexOf('\n');
    while (newlineIndex >= 0)
    {
        const QByteArray line = m_eventBuffer.left(newlineIndex).trimmed();
        m_eventBuffer.remove(0, newlineIndex + 1);
        if (!line.isEmpty())
            handleEventLine(line);
        newlineIndex = m_eventBuffer.indexOf('\n');
    }
}

void DockModel::handleEventLine(const QByteArray &line)
{
    const int separatorIndex = line.indexOf(">>");
    if (separatorIndex <= 0)
        return;

    static const QSet<QByteArray> relevantEvents = {
        QByteArrayLiteral("activewindow"),
        QByteArrayLiteral("activewindowv2"),
        QByteArrayLiteral("windowtitle"),
        QByteArrayLiteral("windowtitlev2"),
        QByteArrayLiteral("openwindow"),
        QByteArrayLiteral("closewindow"),
        QByteArrayLiteral("movewindow"),
        QByteArrayLiteral("movewindowv2"),
        QByteArrayLiteral("workspace"),
        QByteArrayLiteral("workspacev2"),
        QByteArrayLiteral("focusedmon"),
        QByteArrayLiteral("focusedmonv2"),
        QByteArrayLiteral("createworkspace"),
        QByteArrayLiteral("createworkspacev2"),
        QByteArrayLiteral("destroyworkspace"),
        QByteArrayLiteral("destroyworkspacev2"),
        QByteArrayLiteral("moveworkspace"),
        QByteArrayLiteral("moveworkspacev2"),
        QByteArrayLiteral("fullscreen"),
        QByteArrayLiteral("monitoradded"),
        QByteArrayLiteral("monitoraddedv2"),
        QByteArrayLiteral("monitorremoved")
    };

    const QByteArray eventName = line.left(separatorIndex).trimmed();
    if (relevantEvents.contains(eventName))
        m_eventRefreshTimer.start();
}

void DockModel::discoverDesktopEntries()
{
    const QStringList applicationDirectories =
        QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    QSet<QString> visitedFiles;

    for (const QString &directory : applicationDirectories)
    {
        QDirIterator iterator(directory,
                              {QStringLiteral("*.desktop")},
                              QDir::Files,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext())
        {
            const QString path = iterator.next();
            const QString canonicalPath = QFileInfo(path).canonicalFilePath();
            if (canonicalPath.isEmpty() || visitedFiles.contains(canonicalPath))
                continue;
            visitedFiles.insert(canonicalPath);

            QSettings desktopFile(path, QSettings::IniFormat);
            desktopFile.beginGroup(QStringLiteral("Desktop Entry"));
            if (desktopFile.value(QStringLiteral("Type"), QStringLiteral("Application")).toString()
                    != QLatin1String("Application")
                || desktopFile.value(QStringLiteral("Hidden"), false).toBool()
                || desktopFile.value(QStringLiteral("NoDisplay"), false).toBool())
            {
                desktopFile.endGroup();
                continue;
            }

            DesktopEntry entry;
            entry.id = QFileInfo(path).completeBaseName();
            entry.name = desktopFile.value(QStringLiteral("Name"), entry.id).toString().trimmed();
            entry.icon = desktopFile.value(QStringLiteral("Icon"),
                                           QStringLiteral("application-x-executable"))
                             .toString().trimmed();
            entry.executable = desktopFile.value(QStringLiteral("Exec")).toString().trimmed();
            entry.startupWmClass =
                desktopFile.value(QStringLiteral("StartupWMClass")).toString().trimmed();
            entry.terminal = desktopFile.value(QStringLiteral("Terminal"), false).toBool();
            desktopFile.endGroup();

            if (entry.executable.isEmpty() || m_desktopEntries.contains(entry.id))
                continue;

            m_desktopEntries.insert(entry.id, entry);
            registerDesktopAlias(entry.id, entry.id);
            registerDesktopAlias(entry.name, entry.id);
            registerDesktopAlias(entry.startupWmClass, entry.id);
            registerDesktopAlias(executableFromExec(entry.executable), entry.id);
        }
    }
}

void DockModel::registerDesktopAlias(const QString &alias, const QString &desktopId)
{
    const QString key = normalizedId(alias);
    if (!key.isEmpty() && !m_desktopAliases.contains(key))
        m_desktopAliases.insert(key, desktopId);
}

QString DockModel::desktopIdForWindowClass(const QString &windowClass) const
{
    const QString key = normalizedId(windowClass);
    const auto exact = m_desktopAliases.constFind(key);
    if (exact != m_desktopAliases.constEnd())
        return exact.value();

    for (auto iterator = m_desktopAliases.constBegin(); iterator != m_desktopAliases.constEnd(); ++iterator)
    {
        if (iterator.key().size() >= 4
            && (key.contains(iterator.key()) || iterator.key().contains(key)))
        {
            return iterator.value();
        }
    }
    return {};
}

void DockModel::rebuild(const QJsonArray &clients)
{
    QHash<QString, DockEntry> runningEntries;
    QSet<int> fullscreenMonitorIds;
    QSet<QString> reportedAddresses;
    const auto isFullscreen = [](const QJsonValue &value) {
        return value.isBool() ? value.toBool() : value.toInt() > 0;
    };

    for (const QJsonValue &value : clients)
    {
        const QString address = value.toObject()
                                    .value(QStringLiteral("address"))
                                    .toString()
                                    .trimmed();
        if (!address.isEmpty())
            reportedAddresses.insert(address);
    }
    for (auto iterator = m_unfocusableWindowAddresses.begin();
         iterator != m_unfocusableWindowAddresses.end();)
    {
        if (!reportedAddresses.contains(*iterator))
            iterator = m_unfocusableWindowAddresses.erase(iterator);
        else
            ++iterator;
    }

    for (const QJsonValue &value : clients)
    {
        const QJsonObject client = value.toObject();
        if (client.value(QStringLiteral("address")).toString().trimmed()
            != m_activeWindowAddress)
            continue;

        if (isFullscreen(client.value(QStringLiteral("fullscreen")))
            || isFullscreen(client.value(QStringLiteral("fullscreenClient"))))
        {
            fullscreenMonitorIds.insert(client.value(QStringLiteral("monitor")).toInt(-1));
        }
        break;
    }

    const bool fullscreenStateChanged = fullscreenMonitorIds != m_fullscreenMonitorIds;
    m_fullscreenMonitorIds = fullscreenMonitorIds;
    m_fullscreenActive = !m_fullscreenMonitorIds.isEmpty();
    if (fullscreenStateChanged)
    {
        emit fullscreenActiveChanged();
    }

    for (const QJsonValue &value : clients)
    {
        const QJsonObject client = value.toObject();
        const QString clientAddress =
            client.value(QStringLiteral("address")).toString().trimmed();
        if (clientAddress.isEmpty()
            || m_unfocusableWindowAddresses.contains(clientAddress))
        {
            continue;
        }

        QString windowClass = client.value(QStringLiteral("class")).toString().trimmed();
        if (windowClass.isEmpty())
            windowClass = client.value(QStringLiteral("initialClass")).toString().trimmed();
        if (windowClass.isEmpty())
            continue;

        QString appId = desktopIdForWindowClass(windowClass);
        if (appId.isEmpty())
            appId = normalizedId(windowClass);
        if (appId.isEmpty())
            continue;

        DockEntry &entry = runningEntries[appId];
        if (entry.appId.isEmpty())
        {
            entry.appId = appId;
            const auto desktop = m_desktopEntries.constFind(appId);
            if (desktop != m_desktopEntries.constEnd())
            {
                entry.name = desktop->name;
                entry.icon = desktop->icon;
                entry.executable = desktop->executable;
                entry.terminal = desktop->terminal;
            }
            else
            {
                entry.name = displayNameForClass(windowClass);
                entry.icon = QStringLiteral("application-x-executable");
            }
        }

        DockEntry::Window window;
        window.address = clientAddress;
        window.active = clientAddress == m_activeWindowAddress;
        if (!window.address.isEmpty())
            entry.windows.append(window);
        entry.active = entry.active || window.active;
    }

    QList<DockEntry> nextEntries;
    QSet<QString> added;
    for (const QString &configuredId : std::as_const(m_pinnedIds))
    {
        QString appId = configuredId;
        if (!m_desktopEntries.contains(appId))
        {
            const auto alias = m_desktopAliases.constFind(normalizedId(appId));
            if (alias != m_desktopAliases.constEnd())
                appId = alias.value();
        }

        DockEntry entry = runningEntries.take(appId);
        if (entry.appId.isEmpty())
        {
            const auto desktop = m_desktopEntries.constFind(appId);
            if (desktop == m_desktopEntries.constEnd())
                continue;

            entry.appId = appId;
            entry.name = desktop->name;
            entry.icon = desktop->icon;
            entry.executable = desktop->executable;
            entry.terminal = desktop->terminal;
        }
        entry.pinned = true;
        nextEntries.append(entry);
        added.insert(appId);
    }

    QList<DockEntry> unpinned = runningEntries.values();
    std::ranges::sort(unpinned, [](const DockEntry &left, const DockEntry &right) {
        return left.name.localeAwareCompare(right.name) < 0;
    });
    for (DockEntry &entry : unpinned)
    {
        if (added.contains(entry.appId))
            continue;

        nextEntries.append(entry);
    }

    bool sameRowIdentity = m_entries.size() == nextEntries.size();
    if (sameRowIdentity)
    {
        for (qsizetype row = 0; row < m_entries.size(); ++row)
        {
            if (m_entries.at(row).appId != nextEntries.at(row).appId)
            {
                sameRowIdentity = false;
                break;
            }
        }
    }

    if (sameRowIdentity)
    {
        m_entries = std::move(nextEntries);
        if (!m_entries.isEmpty())
        {
            emit dataChanged(index(0, 0),
                             index(static_cast<int>(m_entries.size() - 1), 0));
        }
        return;
    }

    beginResetModel();
    m_entries = std::move(nextEntries);
    endResetModel();
}

void DockModel::applyClientReply(int exitCode, QProcess::ExitStatus exitStatus)
{
    const bool available = exitStatus == QProcess::NormalExit && exitCode == 0;
    if (available != m_compositorAvailable)
    {
        m_compositorAvailable = available;
        emit compositorAvailableChanged();
    }

    if (!available)
    {
        m_pendingClients = {};
        m_clientsReady = true;
        rebuildWhenReady();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(m_clientsProcess.readAllStandardOutput(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray())
    {
        m_pendingClients = {};
        m_clientsReady = true;
        rebuildWhenReady();
        return;
    }

    m_pendingClients = document.array();
    m_clientsReady = true;
    rebuildWhenReady();
}

void DockModel::applyMonitorReply(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_monitorNames.clear();
    if (exitStatus == QProcess::NormalExit && exitCode == 0)
    {
        QJsonParseError parseError;
        const QJsonDocument document =
            QJsonDocument::fromJson(m_monitorsProcess.readAllStandardOutput(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isArray())
        {
            for (const QJsonValue &value : document.array())
            {
                const QJsonObject monitor = value.toObject();
                const int monitorId = monitor.value(QStringLiteral("id")).toInt(-1);
                const QString monitorName = monitor.value(QStringLiteral("name")).toString();
                if (monitorId >= 0 && !monitorName.isEmpty())
                    m_monitorNames.insert(monitorId, monitorName);

            }
        }
    }

    m_monitorsReady = true;
    rebuildWhenReady();
}

void DockModel::applyActiveWindowReply(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_pendingActiveWindowAddress.clear();
    if (exitStatus == QProcess::NormalExit && exitCode == 0)
    {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(
            m_activeWindowProcess.readAllStandardOutput(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject())
        {
            m_pendingActiveWindowAddress = document.object()
                                               .value(QStringLiteral("address"))
                                               .toString()
                                               .trimmed();
        }
    }

    m_activeWindowReady = true;
    rebuildWhenReady();
}

void DockModel::rebuildWhenReady()
{
    if (!m_clientsReady || !m_monitorsReady || !m_activeWindowReady)
        return;

    m_lastClients = m_pendingClients;
    m_activeWindowAddress = m_pendingActiveWindowAddress;
    rebuild(m_lastClients);
    m_clientsReady = false;
    m_monitorsReady = false;
    m_activeWindowReady = false;
    if (m_refreshPending)
        QTimer::singleShot(0, this, &DockModel::refresh);
}

bool DockModel::launch(const DockEntry &entry)
{
    const QStringList command = commandFromExec(entry.executable);
    if (command.isEmpty())
        return false;

    QString program = command.constFirst();
    QStringList arguments = command.sliced(1);
    if (entry.terminal)
    {
        arguments.prepend(program);
        program = QStringLiteral("xdg-terminal-exec");
    }

    const bool started = QProcess::startDetached(program, arguments);
    if (started)
        QTimer::singleShot(500, this, &DockModel::refresh);
    return started;
}

QString DockModel::normalizedId(const QString &value)
{
    QString result;
    result.reserve(value.size());
    for (const QChar character : value.toLower())
    {
        if (character.isLetterOrNumber())
            result.append(character);
    }
    return result;
}

QString DockModel::executableFromExec(const QString &exec)
{
    const QStringList command = commandFromExec(exec);
    return command.isEmpty() ? QString() : QFileInfo(command.constFirst()).fileName();
}

QStringList DockModel::commandFromExec(const QString &exec)
{
    QStringList command = QProcess::splitCommand(exec);
    for (qsizetype index = command.size() - 1; index >= 0; --index)
    {
        QString &argument = command[index];
        if (argument.size() == 2 && argument.startsWith(QLatin1Char('%')))
        {
            command.removeAt(index);
            continue;
        }

        static const QStringList fieldCodes = {
            QStringLiteral("%f"), QStringLiteral("%F"), QStringLiteral("%u"),
            QStringLiteral("%U"), QStringLiteral("%d"), QStringLiteral("%D"),
            QStringLiteral("%n"), QStringLiteral("%N"), QStringLiteral("%i"),
            QStringLiteral("%c"), QStringLiteral("%k"), QStringLiteral("%v"),
            QStringLiteral("%m")
        };
        for (const QString &fieldCode : fieldCodes)
            argument.remove(fieldCode);
        argument.replace(QStringLiteral("%%"), QStringLiteral("%"));
    }
    command.removeAll(QString());
    return command;
}

QString DockModel::displayNameForClass(const QString &windowClass)
{
    QString name = windowClass;
    name.replace(QLatin1Char('-'), QLatin1Char(' '));
    name.replace(QLatin1Char('_'), QLatin1Char(' '));
    if (!name.isEmpty())
        name[0] = name[0].toUpper();
    return name;
}
