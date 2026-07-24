// SPDX-License-Identifier: BSD-3-Clause

#include <QDate>
#include <QDir>
#include <QGuiApplication>
#include <QHash>
#include <QIcon>
#include <QLockFile>
#include <QMargins>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlError>
#include <QScreen>
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QWindow>

#include <utility>

#include <KAboutData>
#include <KLocalizedContext>
#include <KLocalizedString>

#include <LayerShellQt/Window>
#include <MauiKit4/Core/mauiapp.h>

#include "controllers/dockmodel.h"

namespace
{
void configureLayerShellWindow(QWindow *window, DockModel *model, bool activeScreen)
{
    if (!window || !model)
        return;

    auto *layerWindow = LayerShellQt::Window::get(window);
    if (!layerWindow)
        return;

    layerWindow->setScope(QStringLiteral("org.maui.marina"));
    layerWindow->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityNone);
    layerWindow->setWantsToBeOnActiveScreen(activeScreen);
    if (!activeScreen && window->screen())
        layerWindow->setScreen(window->screen());

    LayerShellQt::Window::Anchors anchors;
    anchors |= LayerShellQt::Window::AnchorBottom;
    layerWindow->setAnchors(anchors);
    const auto updateGeometry = [window, layerWindow, model]() {
        layerWindow->setLayer(model->showAboveFullscreen()
                                  ? LayerShellQt::Window::LayerOverlay
                                  : LayerShellQt::Window::LayerTop);
        layerWindow->setExclusiveZone(
            model->autoHide() ? 0 : model->dockHeight() + model->edgeMargin());
        const bool collapsed = model->autoHide() && window->height() < model->dockHeight();
        const int bottomMargin = collapsed ? 0 : model->edgeMargin();
        layerWindow->setMargins(QMargins(0, 0, 0, bottomMargin));
        layerWindow->setDesiredSize(window->size());
    };
    updateGeometry();

    QObject::connect(window, &QWindow::widthChanged, window, updateGeometry);
    QObject::connect(window, &QWindow::heightChanged, window, updateGeometry);
    QObject::connect(model, &DockModel::dockHeightChanged, window, updateGeometry);
    QObject::connect(model, &DockModel::edgeMarginChanged, window, updateGeometry);
    QObject::connect(model, &DockModel::autoHideChanged, window, updateGeometry);
    QObject::connect(model,
                     &DockModel::showAboveFullscreenChanged,
                     window,
                     updateGeometry);
}
}

int main(int argc, char *argv[])
{
    KLocalizedString::setApplicationDomain("marina");

    QSurfaceFormat format;
    format.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);
    QGuiApplication application(argc, argv);
    application.setQuitOnLastWindowClosed(false);
    application.setApplicationName(QStringLiteral("marina"));
    application.setApplicationDisplayName(QStringLiteral("Marina"));
    application.setApplicationVersion(QStringLiteral("0.1.0"));
    application.setDesktopFileName(QStringLiteral("org.maui.marina"));
    application.setOrganizationName(QStringLiteral("Maui"));
    application.setOrganizationDomain(QStringLiteral("maui.org"));
    application.setWindowIcon(QIcon::fromTheme(QStringLiteral("user-desktop")));

    QString instanceDirectory =
        QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (instanceDirectory.isEmpty())
    {
        instanceDirectory =
            QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    }
    if (instanceDirectory.isEmpty() || !QDir().mkpath(instanceDirectory))
    {
        qCritical() << "Marina could not create its single-instance lock directory.";
        return -1;
    }

    QLockFile instanceLock(
        QDir(instanceDirectory).filePath(QStringLiteral("org.maui.marina.lock")));
    if (!instanceLock.tryLock())
    {
        if (instanceLock.error() == QLockFile::LockFailedError)
        {
            qInfo() << "Marina is already running.";
            return 0;
        }

        qCritical() << "Marina could not acquire its single-instance lock.";
        return -1;
    }

    KAboutData aboutData(
        QStringLiteral("marina"),
        i18n("Marina"),
        QStringLiteral("0.1.0"),
        i18n("Workspace dock for Nitrux built with MauiKit and LayerShell-Qt."),
        KAboutLicense::BSD_3_Clause,
        i18n("© %1 Nitrux Latinoamericana S.C.", QString::number(QDate::currentDate().year())));
    aboutData.setHomepage(QStringLiteral("https://github.com/Nitrux/marina"));
    aboutData.setBugAddress(QByteArrayLiteral("https://github.com/Nitrux/marina/issues"));
    aboutData.setDesktopFileName(QByteArrayLiteral("org.maui.marina"));
    aboutData.setOrganizationDomain(QByteArrayLiteral("org.maui.marina"));
    aboutData.setProgramLogo(application.windowIcon());
    KAboutData::setApplicationData(aboutData);

    MauiApp::instance();
    MauiApp::instance()->setIconName(QStringLiteral("user-desktop"));

    DockModel dockModel;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
    engine.rootContext()->setContextProperty(QStringLiteral("dockModel"), &dockModel);

    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/app/marina/main.qml")));
    if (component.isError())
    {
        for (const QQmlError &error : component.errors())
            qWarning().noquote() << error.toString();
        return -1;
    }

    bool allScreens = dockModel.screenPlacement() == QLatin1String("all");
    QHash<QScreen *, QPointer<QWindow>> screenWindows;
    QPointer<QWindow> activeScreenWindow;

    const auto createDock = [&](QScreen *screen) -> QWindow * {
        if (allScreens && screen && screenWindows.value(screen))
            return screenWindows.value(screen);
        if (!allScreens && activeScreenWindow)
            return activeScreenWindow;

        QObject *object = component.create(engine.rootContext());
        auto *window = qobject_cast<QWindow *>(object);
        if (!window)
        {
            delete object;
            for (const QQmlError &error : component.errors())
                qWarning().noquote() << error.toString();
            return nullptr;
        }

        if (screen)
            window->setScreen(screen);
        window->close();
        configureLayerShellWindow(window, &dockModel, !allScreens);
        window->show();

        const auto updateFullscreenVisibility = [window, &dockModel]() {
            if (dockModel.showAboveFullscreen())
            {
                window->setVisible(true);
                return;
            }

            const QString screenName =
                window->screen() ? window->screen()->name() : QString();
            window->setVisible(!dockModel.fullscreenActiveOnScreen(screenName));
        };
        QObject::connect(&dockModel,
                         &DockModel::fullscreenActiveChanged,
                         window,
                         updateFullscreenVisibility);
        QObject::connect(&dockModel,
                         &DockModel::showAboveFullscreenChanged,
                         window,
                         updateFullscreenVisibility);
        QObject::connect(window,
                         &QWindow::screenChanged,
                         window,
                         updateFullscreenVisibility);
        updateFullscreenVisibility();

        if (allScreens && screen)
        {
            screenWindows.insert(screen, window);
            QObject::connect(window, &QObject::destroyed, &application, [&, screen, window]() {
                if (screenWindows.value(screen) == window)
                    screenWindows.remove(screen);
            });
        }
        else
        {
            activeScreenWindow = window;
            QObject::connect(window, &QObject::destroyed, &application, [&, window]() {
                if (activeScreenWindow == window)
                    activeScreenWindow.clear();
            });
        }
        return window;
    };

    const auto rebuildDocks = [&]() {
        for (const QPointer<QWindow> &window : std::as_const(screenWindows))
        {
            if (window)
            {
                window->close();
                window->deleteLater();
            }
        }
        screenWindows.clear();
        if (activeScreenWindow)
        {
            activeScreenWindow->close();
            activeScreenWindow->deleteLater();
            activeScreenWindow.clear();
        }

        allScreens = dockModel.screenPlacement() == QLatin1String("all");
        if (allScreens)
        {
            for (QScreen *screen : application.screens())
                createDock(screen);
        }
        else
        {
            createDock(nullptr);
        }
    };

    rebuildDocks();

    QObject::connect(&dockModel,
                     &DockModel::screenPlacementChanged,
                     &application,
                     rebuildDocks);
    QObject::connect(&application, &QGuiApplication::screenAdded, &application,
                     [&](QScreen *screen) {
                         if (allScreens)
                             createDock(screen);
                     });
    QObject::connect(&application, &QGuiApplication::screenRemoved, &application,
                     [&](QScreen *screen) {
                         if (screenWindows.value(screen))
                         {
                             screenWindows.value(screen)->close();
                             screenWindows.value(screen)->deleteLater();
                         }
                         screenWindows.remove(screen);
                     });

    return application.exec();
}
