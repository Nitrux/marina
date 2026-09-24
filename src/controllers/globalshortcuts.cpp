// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 Nitrux Latinoamericana S.C. <hello@nxos.org>

#include "globalshortcuts.h"
#include "dockmodel.h"

#include <QLoggingCategory>
#include <QList>
#include <QString>
#include <QWaylandClientExtensionTemplate>

#include <qwayland-hyprland-global-shortcuts-v1.h>

Q_LOGGING_CATEGORY(marinaShortcutsLog, "marina.shortcuts")

namespace Marina::Private
{
class GlobalShortcutManager final
    : public QWaylandClientExtensionTemplate<GlobalShortcutManager>
    , public QtWayland::hyprland_global_shortcuts_manager_v1
{
public:
    GlobalShortcutManager()
        : QWaylandClientExtensionTemplate<GlobalShortcutManager>(1)
    {
        initialize();
    }
};

class GlobalShortcut final : public QObject,
                             public QtWayland::hyprland_global_shortcut_v1
{
    Q_OBJECT

public:
    explicit GlobalShortcut(::hyprland_global_shortcut_v1 *shortcut,
                            const QString &id,
                            QObject *parent = nullptr)
        : QObject(parent)
        , QtWayland::hyprland_global_shortcut_v1(shortcut)
        , m_id(id)
    {
    }

    void resetPressed()
    {
        m_pressed = false;
    }

    ~GlobalShortcut() override
    {
        if (isInitialized())
            destroy();
    }

signals:
    void pressed();
    void released();

private:
    QString m_id;
    bool m_pressed = false;
    void hyprland_global_shortcut_v1_pressed(quint32 tvSecHi,
                                             quint32 tvSecLo,
                                             quint32 tvNsec) override
    {
        if (m_pressed)
            return;

        qCDebug(marinaShortcutsLog).noquote()
            << m_id << "protocol pressed" << tvSecHi << tvSecLo << tvNsec;
        m_pressed = true;
        emit pressed();
    }

    void hyprland_global_shortcut_v1_released(quint32 tvSecHi,
                                              quint32 tvSecLo,
                                              quint32 tvNsec) override
    {
        qCDebug(marinaShortcutsLog).noquote()
            << m_id << "protocol released" << tvSecHi << tvSecLo << tvNsec
            << "wasPressed=" << m_pressed;
        m_pressed = false;
        emit released();
    }
};
}

namespace Marina
{
namespace
{
constexpr auto kApplicationId = "org.maui.marina";
constexpr auto kHoldShortcutId = "launcher-hold";
constexpr int kLauncherShortcutCount = 10;
constexpr int kDefaultLauncherHoldDelay = 3000;
constexpr int kDefaultLauncherModeDuration = 1000;
}

GlobalShortcutController::GlobalShortcutController(DockModel *model, QObject *parent)
    : QObject(parent)
    , m_manager(new Private::GlobalShortcutManager)
{
    m_manager->setParent(this);
    m_holdTimer.setTimerType(Qt::PreciseTimer);
    m_holdTimer.setSingleShot(true);
    m_holdTimer.setInterval(model ? model->launcherHoldDelay() : kDefaultLauncherHoldDelay);
    m_launcherModeTimer.setTimerType(Qt::PreciseTimer);
    m_launcherModeTimer.setSingleShot(true);
    m_launcherModeTimer.setInterval(model ? model->launcherModeDuration() : kDefaultLauncherModeDuration);
    if (model)
    {
        connect(model, &DockModel::launcherHoldDelayChanged, this, [this, model]() {
            m_holdTimer.setInterval(model->launcherHoldDelay());
        });
        connect(model, &DockModel::launcherModeDurationChanged, this, [this, model]() {
            m_launcherModeTimer.setInterval(model->launcherModeDuration());
        });
        connect(model, &DockModel::workspaceChanged, this, [this]() {
            if (!m_holdTimer.isActive() || m_launcherMode)
                return;

            m_holdTimer.stop();
            m_superHeld = false;
            if (m_holdShortcut)
                m_holdShortcut->resetPressed();
        });
    }
    connect(&m_holdTimer, &QTimer::timeout, this, [this]() {
        if (!m_superHeld)
            return;

        m_launcherMode = true;
        emit launcherModeChanged(true);
        m_launcherModeTimer.start();
    });
    connect(&m_launcherModeTimer, &QTimer::timeout, this, [this]() {
        if (!m_launcherMode)
            return;

        m_launcherMode = false;
        m_superHeld = false;
        if (m_holdShortcut)
            m_holdShortcut->resetPressed();
        emit launcherModeChanged(false);
    });
    connect(m_manager, &QWaylandClientExtension::activeChanged, this, [this]() {
        qCDebug(marinaShortcutsLog)
            << "Wayland shortcut manager activeChanged"
            << "active=" << m_manager->isActive();
        registerShortcuts();
    });
    registerShortcuts();
}

void GlobalShortcutController::handleSuperPressed()
{
    qCDebug(marinaShortcutsLog)
        << "controller Super pressed" << "superHeld=" << m_superHeld
        << "launcherMode=" << m_launcherMode;
    if (m_superHeld)
    {
        qCDebug(marinaShortcutsLog) << "controller Super press ignored as duplicate";
        return;
    }

    m_superHeld = true;
    m_holdTimer.start();
}

void GlobalShortcutController::handleSuperReleased()
{
    qCDebug(marinaShortcutsLog)
        << "controller Super released" << "superHeld=" << m_superHeld
        << "launcherMode=" << m_launcherMode;
    if (!m_superHeld)
    {
        qCDebug(marinaShortcutsLog) << "controller Super release ignored without active press";
        return;
    }

    m_superHeld = false;
    m_holdTimer.stop();
    if (m_holdShortcut)
        m_holdShortcut->resetPressed();
}

void GlobalShortcutController::handleLauncherPressed(int index)
{
    qCDebug(marinaShortcutsLog)
        << "controller launcher pressed" << "index=" << index
        << "superHeld=" << m_superHeld
        << "launcherMode=" << m_launcherMode;
    if (m_launcherMode)
        emit launcherPressed(index);
}

void GlobalShortcutController::registerShortcuts()
{
    qCDebug(marinaShortcutsLog)
        << "registerShortcuts" << "registered=" << m_registered
        << "active=" << m_manager->isActive();
    if (m_registered || !m_manager->isActive())
    {
        qCDebug(marinaShortcutsLog) << "shortcut registration skipped";
        return;
    }

    auto registerShortcut = [this](const QString &id, const QString &description) {
        qCDebug(marinaShortcutsLog) << "registering shortcut" << id;
        auto *shortcut = m_manager->register_shortcut(id,
                                                       QString::fromLatin1(kApplicationId),
                                                       description,
                                                       QString());
        if (!shortcut)
        {
            qCDebug(marinaShortcutsLog) << "shortcut registration returned null" << id;
            return static_cast<Private::GlobalShortcut *>(nullptr);
        }

        auto *clientShortcut = new Private::GlobalShortcut(shortcut, id, this);
        m_shortcuts.append(clientShortcut);
        qCDebug(marinaShortcutsLog) << "shortcut registered" << id;
        return clientShortcut;
    };

    if (auto *holdShortcut = registerShortcut(
            QString::fromLatin1(kHoldShortcutId),
            QStringLiteral("Hold Super to show launcher numbers")))
    {
        m_holdShortcut = holdShortcut;
        connect(holdShortcut,
                &Private::GlobalShortcut::pressed,
                this,
                &GlobalShortcutController::handleSuperPressed);
        connect(holdShortcut,
                &Private::GlobalShortcut::released,
                this,
                &GlobalShortcutController::handleSuperReleased);
    }

    for (int index = 0; index < kLauncherShortcutCount; ++index)
    {
        auto *launcherShortcut = registerShortcut(
            QStringLiteral("launcher-%1").arg(index + 1),
            QStringLiteral("Open pinned launcher %1").arg(index == 9 ? 0 : index + 1));
        if (!launcherShortcut)
            continue;

        connect(launcherShortcut,
                &Private::GlobalShortcut::pressed,
                this,
                [this, index]() { handleLauncherPressed(index); });
    }

    m_registered = true;
    qCDebug(marinaShortcutsLog)
        << "shortcut registration complete" << "count=" << m_shortcuts.size();
}
}

#include "globalshortcuts.moc"
