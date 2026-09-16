// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import Qt.labs.platform as Platform
import WebAppContainer

QtObject {
    id: root
    required property Window mainWindow
    required property Window downloadWindow
    required property DownloadModel downloadModel
    required property LaunchConfiguration launchConfiguration
    required property SettingsStore settingsStore
    required property AppController appController
    required property NotificationModel notificationModel
    required property NotificationPresenter notificationPresenter
    readonly property bool available: tray.available

    function restoreWindow() {
        root.appController.restoreWindow(root.mainWindow,
                                         root.settingsStore.windowMaximized)
    }

    function toggleWindow() {
        if (!root.mainWindow.visible || root.mainWindow.visibility === Window.Minimized) {
            root.restoreWindow()
        } else if (root.mainWindow.active) {
            root.mainWindow.hide()
        } else {
            root.mainWindow.raise()
            root.mainWindow.requestActivate()
        }
    }

    function showBackgroundMessage(title, message) {
        if (root.launchConfiguration.showBackgroundHints && tray.supportsMessages)
            tray.showMessage(title, message,
                             Platform.SystemTrayIcon.Information, 2000)
    }

    property Platform.SystemTrayIcon tray: Platform.SystemTrayIcon {
        id: systemTrayIcon
        objectName: "systemTrayIcon"
        visible: true
        icon.source: root.appController.trayIconUrl
        tooltip: root.launchConfiguration.applicationName
        property var activeNotificationId: 0

        menu: Platform.Menu {
            objectName: "systemTrayMenu"
            visible: false

            Platform.MenuItem {
                text: qsTr("Restore")
                onTriggered: root.restoreWindow()
            }
            Platform.MenuItem {
                text: qsTr("Downloads")
                enabled: root.downloadModel.count > 0
                onTriggered: {
                    root.downloadWindow.show()
                    root.downloadWindow.raise()
                    root.downloadWindow.requestActivate()
                }
            }
            Platform.MenuSeparator {}
            Platform.MenuItem {
                id: hideMinimizedItem
                text: qsTr("Minimize to Tray")
                checkable: true
                checked: root.settingsStore.hideOnMinimize
                onTriggered: root.settingsStore.hideOnMinimize = hideMinimizedItem.checked
            }
            Platform.MenuItem {
                id: hideClosedItem
                text: qsTr("Close to Tray")
                checkable: true
                checked: root.settingsStore.hideOnClose
                onTriggered: root.settingsStore.hideOnClose = hideClosedItem.checked
            }
            Platform.MenuSeparator {}
            Platform.MenuItem {
                text: qsTr("Exit")
                role: Platform.MenuItem.QuitRole
                onTriggered: root.appController.requestQuit()
            }
        }

        onActivated: function(reason) {
            if (reason === Platform.SystemTrayIcon.Trigger)
                root.toggleWindow()
        }
        onMessageClicked: {
            if (systemTrayIcon.activeNotificationId !== 0) {
                root.notificationModel.click(systemTrayIcon.activeNotificationId)
                systemTrayIcon.activeNotificationId = 0
            }
        }
    }

    property Connections notificationConnections: Connections {
        target: root.notificationPresenter
        function onFallbackRequested(id, title, message) {
            systemTrayIcon.activeNotificationId = id
            systemTrayIcon.showMessage(title, message,
                                       Platform.SystemTrayIcon.Information, 10000)
        }
    }
    property Connections notificationLifecycleConnections: Connections {
        target: root.notificationModel
        function onNotificationRemoved(id) {
            if (systemTrayIcon.activeNotificationId === id)
                systemTrayIcon.activeNotificationId = 0
        }
    }
}
