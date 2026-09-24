// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 Nitrux Latinoamericana S.C. <hello@nxos.org>

#pragma once

#include <QList>
#include <QObject>
#include <QTimer>

class DockModel;

namespace Marina
{
namespace Private
{
class GlobalShortcutManager;
class GlobalShortcut;
}

class GlobalShortcutController final : public QObject
{
    Q_OBJECT

public:
    explicit GlobalShortcutController(DockModel *model, QObject *parent = nullptr);

signals:
    void launcherModeChanged(bool active);
    void launcherPressed(int index);

private:
    void registerShortcuts();
    void handleSuperPressed();
    void handleSuperReleased();
    void handleLauncherPressed(int index);

    Private::GlobalShortcutManager *m_manager = nullptr;
    Private::GlobalShortcut *m_holdShortcut = nullptr;
    QList<Private::GlobalShortcut *> m_shortcuts;
    bool m_registered = false;
    bool m_superHeld = false;
    bool m_launcherMode = false;
    QTimer m_holdTimer;
    QTimer m_launcherModeTimer;
};
}
