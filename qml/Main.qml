// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import WebAppContainer

ApplicationWindow {
    id: root
    required property LaunchConfiguration launchConfiguration
    required property ProfileService profileService
    required property SettingsStore settingsStore
    required property AppController appController
    required property NavigationPolicy navigationPolicy
    required property SecurityBridge securityBridge
    required property DownloadModel downloadModel
    required property NotificationModel notificationModel
    required property NotificationPresenter notificationPresenter

    PopupManager {
        id: popupManager
        navigationPolicy: root.navigationPolicy
        securityBridge: root.securityBridge
        settingsStore: root.settingsStore
        pushSubscriptionScript: root.launchConfiguration.pushSubscriptionScript
    }

    width: 967
    height: 557
    visible: false
    title: root.launchConfiguration.applicationName

    AppWebView {
        anchors.fill: parent
        // Propagate window hiding to Chromium without suspending the page.
        visible: root.visibility !== Window.Hidden &&
                 root.visibility !== Window.Minimized
        // QQuickWebEngineProfile is the C++ backing type of WebEngineProfile.
        // qmllint disable unresolved-type
        webProfile: root.profileService.profile
        // qmllint enable unresolved-type
        navigationPolicy: root.navigationPolicy
        securityBridge: root.securityBridge
        settingsStore: root.settingsStore
        pushSubscriptionScript: root.launchConfiguration.pushSubscriptionScript
        popupManager: popupManager
        url: root.launchConfiguration.startUrl
    }

    readonly property bool trayAvailable: root.appController.nativeTrayAvailable

    function showTrayMessage(title, message) {
        if (!trayLoader.item)
            return
        // qmllint disable missing-property
        trayLoader.item.showBackgroundMessage(title, message)
        // qmllint enable missing-property
    }

    function completeStartup() {
        if (root.launchConfiguration.startMinimized && root.trayAvailable)
            return
        if (root.settingsStore.windowMaximized)
            root.showMaximized()
        else
            root.showNormal()
    }

    Loader {
        id: trayLoader
        active: root.appController.nativeTrayAvailable
        sourceComponent: TrayController {
            // qmllint disable unqualified
            mainWindow: root
            launchConfiguration: root.launchConfiguration
            settingsStore: root.settingsStore
            appController: root.appController
            notificationModel: root.notificationModel
            notificationPresenter: root.notificationPresenter
            downloadWindow: downloadsPopup
            downloadModel: root.downloadModel
            // qmllint enable unqualified
        }
    }

    DownloadsWindow {
        id: downloadsPopup
        downloadModel: root.downloadModel
    }

    Connections {
        target: root.notificationModel
        function onNotificationPresented(id, title, message, iconUrl) {
            if (!root.active)
                root.appController.notificationBadge = true
        }
        function onNotificationActivated() {
            root.show()
            root.raise()
            root.requestActivate()
            root.appController.notificationBadge = false
        }
    }

    Connections {
        target: root.downloadModel
        function onCountChanged() {
            if (root.downloadModel.count > 0 && !downloadsPopup.visible) {
                downloadsPopup.show()
                downloadsPopup.raise()
                downloadsPopup.requestActivate()
            }
        }
    }

    onVisibilityChanged: {
        if (root.visibility === Window.Minimized && root.settingsStore.hideOnMinimize &&
                root.trayAvailable) {
            Qt.callLater(root.hide)
            root.showTrayMessage(qsTr("App Minimized"),
                                 qsTr("The application is still running in the system tray."))
        }
    }
    onActiveChanged: {
        if (root.active)
            root.appController.notificationBadge = false
    }
    onClosing: function(close) {
        if (!root.appController.quitting && root.settingsStore.hideOnClose &&
                root.trayAvailable) {
            close.accepted = false
            root.hide()
            root.showTrayMessage(qsTr("Running in background"),
                                 qsTr("The application is still active in the system tray."))
        } else if (!root.appController.quitting) {
            close.accepted = false
            root.appController.requestQuit()
        }
    }
}
