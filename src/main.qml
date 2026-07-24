// SPDX-License-Identifier: BSD-3-Clause

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

import org.mauikit.controls as Maui

Window
{
    id: root

    readonly property int collapsedHeight: 3
    readonly property int availableWidth: Math.max(1, Screen.width - 24)
    readonly property int launcherHoverInset:
        Math.ceil((dockModel.iconSize + 8) * 0.06) + 1
    readonly property int naturalWidth:
        Math.max(1, dockContent.implicitWidth + 20 + (launcherHoverInset * 2))
    property bool autoHideExpanded: true
    property int transientSurfaceCount: 0
    readonly property bool dockExpanded: !dockModel.autoHide
                                         || autoHideExpanded
                                         || transientSurfaceCount > 0
    property real presentationWidth:
        Math.min(availableWidth,
                 dockModel.dockWidth > 0
                 ? Math.max(dockModel.dockWidth, naturalWidth)
                 : naturalWidth)
    property real presentationHeight: dockExpanded
                                      ? dockModel.dockHeight
                                      : collapsedHeight

    visible: false
    width: Math.round(presentationWidth)
    minimumWidth: Math.round(presentationWidth)
    maximumWidth: Math.round(presentationWidth)
    height: Math.round(presentationHeight)
    minimumHeight: Math.round(presentationHeight)
    maximumHeight: Math.round(presentationHeight)
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowDoesNotAcceptFocus
    title: i18n("Marina")

    onTransientSurfaceCountChanged:
    {
        if (transientSurfaceCount > 0)
            autoHideExpanded = true
    }

    Behavior on presentationHeight
    {
        NumberAnimation
        {
            duration: 140
            easing.type: Easing.OutCubic
        }
    }

    HoverHandler
    {
        id: dockHover

        onHoveredChanged:
        {
            if (hovered)
                root.autoHideExpanded = true
        }
    }

    Timer
    {
        id: hideTimer
        interval: dockModel.autoHideDelay
        running: dockModel.autoHide
                 && root.autoHideExpanded
                 && root.transientSurfaceCount === 0
                 && !dockHover.hovered
        onTriggered: root.autoHideExpanded = false
    }

    Connections
    {
        target: dockModel

        function onAutoHideChanged()
        {
            root.autoHideExpanded = true
        }
    }

    Maui.WindowBlur
    {
        view: root
        geometry: Qt.rect(0, 0, root.width, root.height)
        windowRadius: Maui.Style.radiusV + 6
        enabled: true
    }

    Rectangle
    {
        anchors.fill: parent
        opacity: root.dockExpanded ? 1 : 0
        color: Qt.alpha(Maui.Theme.backgroundColor, 0.82)
        border.color: Qt.alpha(Maui.Theme.textColor, 0.14)
        border.width: 1
        radius: Maui.Style.radiusV + 6

        Behavior on opacity { NumberAnimation { duration: 100 } }
    }

    Flickable
    {
        id: dockViewport

        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        clip: true
        contentWidth: Math.max(width,
                               dockContent.implicitWidth + (root.launcherHoverInset * 2))
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        interactive: root.dockExpanded && contentWidth > width

        ScrollBar.horizontal: ScrollBar
        {
            policy: dockViewport.contentWidth > dockViewport.width
                    ? ScrollBar.AsNeeded
                    : ScrollBar.AlwaysOff
        }

        RowLayout
        {
            id: dockContent

            x: root.launcherHoverInset
               + Math.max(0,
                          (dockViewport.width - implicitWidth
                           - (root.launcherHoverInset * 2)) / 2)
            y: Math.round((dockViewport.height - implicitHeight) / 2)
            spacing: Maui.Style.space.small
            enabled: root.dockExpanded
            opacity: root.dockExpanded ? 1 : 0

            Behavior on opacity { NumberAnimation { duration: 100 } }

            Repeater
            {
                model: dockModel

                delegate: Item
                {
                    id: launcher

                    required property int index
                    required property string appId
                    required property string name
                    required property string iconName
                    required property bool running
                    required property bool active
                    required property bool pinned
                    required property int windowCount
                    required property bool launchable
                    required property int activeWindowIndex
                    property real dragOffset: 0
                    property bool hoverSuppressed: false
                    readonly property bool visuallyHovered:
                        pointer.containsMouse && !hoverSuppressed

                    Layout.preferredWidth: dockModel.iconSize + 8
                    Layout.preferredHeight: root.height - 8
                    scale: visuallyHovered ? 1.12 : 1.0
                    z: visuallyHovered ? 2 : 1
                    transform: Translate { x: launcher.dragOffset }

                Behavior on scale
                {
                    NumberAnimation
                    {
                        duration: 120
                        easing.type: Easing.OutCubic
                    }
                }

                Rectangle
                {
                    anchors.fill: parent
                    color: launcher.active
                           ? Qt.alpha(Maui.Theme.highlightColor, 0.18)
                           : launcher.visuallyHovered
                             ? Qt.alpha(Maui.Theme.textColor, 0.08)
                             : "transparent"
                    radius: Maui.Style.radiusV
                }

                Maui.IconItem
                {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: launcher.running ? -2 : 0
                    width: dockModel.iconSize
                    height: dockModel.iconSize
                    iconSource: launcher.iconName
                    iconSizeHint: dockModel.iconSize
                    color: Maui.Theme.textColor
                }

                Row
                {
                    id: windowIndicators

                    readonly property int visibleActiveIndex:
                        launcher.activeWindowIndex < 0
                        ? -1
                        : Math.min(launcher.activeWindowIndex, 2)

                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 2
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 3
                    visible: launcher.running

                    Repeater
                    {
                        model: launcher.windowCount > 3
                               ? 1
                               : launcher.windowCount

                        Rectangle
                        {
                            required property int index
                            width: launcher.windowCount > 3
                                   ? (launcher.active ? 12 : 5)
                                   : index === windowIndicators.visibleActiveIndex
                                     ? 12
                                     : 5
                            height: 3
                            radius: 2
                            color: Maui.Theme.highlightColor

                            Behavior on width
                            {
                                NumberAnimation { duration: 120 }
                            }
                        }
                    }
                }

                ToolButton
                {
                    id: windowCountBadge

                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: 2
                    visible: launcher.windowCount > 3
                    text: String(launcher.windowCount)
                    display: ToolButton.TextOnly
                    hoverEnabled: false
                    focusPolicy: Qt.NoFocus
                    font.bold: true
                    font.pointSize: Maui.Style.fontSizes.small
                    ToolTip.visible: false
                    ToolTip.text: ""

                    background: Rectangle
                    {
                        color: Maui.Theme.alternateBackgroundColor
                        radius: Maui.Style.radiusV
                    }
                }

                MouseArea
                {
                    id: pointer

                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
                    cursorShape: reorderDrag.active ? Qt.ClosedHandCursor : Qt.PointingHandCursor

                    onClicked: (mouse) =>
                    {
                        if (mouse.button === Qt.RightButton)
                            contextMenu.openFromLauncher()
                        else if (mouse.button === Qt.MiddleButton)
                            dockModel.launchNew(launcher.index)
                        else
                            dockModel.trigger(launcher.index)
                    }
                }

                DragHandler
                {
                    id: reorderDrag

                    enabled: launcher.pinned
                    target: null
                    acceptedButtons: Qt.LeftButton
                    xAxis.enabled: true
                    yAxis.enabled: false

                    onActiveTranslationChanged:
                    {
                        if (active)
                            launcher.dragOffset = activeTranslation.x
                    }

                    onActiveChanged:
                    {
                        if (active)
                            return

                        const offset = launcher.dragOffset
                        launcher.dragOffset = 0
                        if (Math.abs(offset) < 8)
                            return

                        const cellWidth = launcher.width + dockContent.spacing
                        const targetRow = Math.round((launcher.x + offset) / cellWidth)
                        dockModel.movePinned(launcher.index, targetRow)
                    }
                }

                Item
                {
                    id: popupAnchorMarker

                    x: Math.round((launcher.width - width) / 2)
                    y:
                    {
                        const dockTopInLauncher = launcher.mapFromItem(root.contentItem, 0, 0)
                        return dockTopInLauncher.y
                    }
                    width: 1
                    height: 1
                    visible: true
                }

                Timer
                {
                    id: popupHoverRestoreTimer

                    interval: 160
                    onTriggered: launcher.hoverSuppressed = false
                }

                Window
                {
                    id: contextMenu

                    property int geometryRevision: 0

                    visible: false
                    screen: root.screen
                    width: 224
                    height: contextMenuColumn.implicitHeight + 12
                    color: "transparent"
                    flags: Qt.FramelessWindowHint | Qt.Popup
                    transientParent: root
                    title: i18n("%1 actions", launcher.name)
                    Maui.Theme.colorSet: Maui.Theme.View

                    function anchorPointInDock()
                    {
                        if (!popupAnchorMarker.mapToItem)
                            return null

                        const point = popupAnchorMarker.mapToItem(root.contentItem, 0, 0)
                        if (point && isFinite(point.x) && isFinite(point.y))
                            return point

                        return null
                    }

                    function dockOriginInScreen()
                    {
                        const geometry = screenGeometry()
                        const bottomMargin = root.height < dockModel.dockHeight
                                ? 0
                                : dockModel.edgeMargin
                        return Qt.point(geometry.x + ((geometry.width - root.width) / 2),
                                        geometry.y + geometry.height
                                        - bottomMargin - root.height)
                    }

                    function screenGeometry()
                    {
                        const targetScreen = root.screen
                        if (targetScreen && targetScreen.geometry
                                && targetScreen.geometry.width > 0
                                && targetScreen.geometry.height > 0)
                            return targetScreen.geometry

                        return Qt.rect(0, 0, Screen.width, Screen.height)
                    }

                    x:
                    {
                        const revision = geometryRevision
                        const geometry = screenGeometry()
                        const anchorPoint = anchorPointInDock()
                        const dockOrigin = dockOriginInScreen()
                        const desiredX = anchorPoint
                                ? anchorPoint.x - (width / 2)
                                : (root.width - width) / 2
                        const originX = dockOrigin ? dockOrigin.x : geometry.x
                        const minimumX = geometry.x - originX + 8
                        const maximumX = Math.max(minimumX,
                                                  geometry.x + geometry.width
                                                  - originX - width - 8)
                        return Math.round(Math.max(minimumX,
                                                  Math.min(maximumX, desiredX)))
                    }

                    y:
                    {
                        const revision = geometryRevision
                        const geometry = screenGeometry()
                        const anchorPoint = anchorPointInDock()
                        const dockOrigin = dockOriginInScreen()
                        const dockTop = anchorPoint ? anchorPoint.y : 0
                        const originY = dockOrigin ? dockOrigin.y : geometry.y
                        const minimumY = geometry.y - originY + 8
                        return Math.round(Math.max(minimumY, dockTop - height))
                    }

                    function openFromLauncher()
                    {
                        popupHoverRestoreTimer.stop()
                        launcher.hoverSuppressed = true
                        geometryRevision += 1
                        Qt.callLater(function() {
                            contextMenu.visible = true
                            contextMenu.requestActivate()
                        })
                    }

                    onVisibleChanged:
                    {
                        if (visible)
                        {
                            root.transientSurfaceCount += 1
                        }
                        else
                        {
                            root.transientSurfaceCount = Math.max(0,
                                                                  root.transientSurfaceCount - 1)
                            popupHoverRestoreTimer.restart()
                        }
                    }

                    Rectangle
                    {
                        anchors.fill: parent
                        color: Qt.rgba(Maui.Theme.backgroundColor.r,
                                       Maui.Theme.backgroundColor.g,
                                       Maui.Theme.backgroundColor.b,
                                       1)
                        border.color: Qt.alpha(Maui.Theme.textColor, 0.14)
                        border.width: 1
                        radius: Maui.Style.radiusV
                    }

                    Column
                    {
                        id: contextMenuColumn

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6

                        MenuItem
                        {
                            width: parent.width
                            text: i18n("Open new window")
                            icon.name: "window-new"
                            enabled: launcher.launchable
                            onTriggered:
                            {
                                contextMenu.close()
                                dockModel.launchNew(launcher.index)
                            }
                        }

                        MenuItem
                        {
                            width: parent.width
                            text: launcher.pinned
                                  ? i18n("Unpin launcher")
                                  : i18n("Pin launcher")
                            icon.name: launcher.pinned ? "window-unpin" : "window-pin"
                            onTriggered:
                            {
                                contextMenu.close()
                                dockModel.togglePinned(launcher.index)
                            }
                        }

                        MenuItem
                        {
                            width: parent.width
                            visible: launcher.running
                            text: i18n("Close")
                            icon.name: "window-close"
                            onTriggered:
                            {
                                contextMenu.close()
                                dockModel.closeWindows(launcher.index)
                            }
                        }
                    }
                }

                }
            }
        }
    }

}
