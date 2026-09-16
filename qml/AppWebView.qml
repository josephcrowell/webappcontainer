// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import QtWebEngine
import WebAppContainer

WebEngineView {
    id: root
    required property var webProfile
    required property NavigationPolicy navigationPolicy
    required property SecurityBridge securityBridge
    required property SettingsStore settingsStore
    required property string pushSubscriptionScript
    required property var popupManager

    profile: root.webProfile
    settings.javascriptEnabled: true
    settings.localStorageEnabled: true
    settings.pluginsEnabled: true
    settings.dnsPrefetchEnabled: true
    settings.localContentCanAccessRemoteUrls: true
    settings.localContentCanAccessFileUrls: false
    settings.screenCaptureEnabled: true
    // QQuickWebEngineView exposes this documented collection through a private
    // C++ backing type that qmllint cannot resolve from the public type file.
    // qmllint disable unqualified unresolved-type
    userScripts.collection: [{
        name: "WebAppContainer Push Subscription Persistence",
        sourceCode: root.pushSubscriptionScript,
        injectionPoint: WebEngineScript.DocumentCreation,
        worldId: WebEngineScript.MainWorld,
        runsOnSubFrames: false
    }]
    // qmllint enable unqualified unresolved-type

    RequestDialogs {
        id: requestDialogs
        anchors.fill: parent
        securityBridge: root.securityBridge
        settingsStore: root.settingsStore
        onReloadRequested: root.reload()
    }

    Menu {
        id: contextMenu
        property url linkUrl
        MenuItem {
            text: qsTr("Back")
            enabled: root.action(WebEngineView.Back).enabled
            onTriggered: root.triggerWebAction(WebEngineView.Back)
        }
        MenuItem {
            text: qsTr("Forward")
            enabled: root.action(WebEngineView.Forward).enabled
            onTriggered: root.triggerWebAction(WebEngineView.Forward)
        }
        MenuItem {
            text: qsTr("Reload")
            enabled: root.action(WebEngineView.Reload).enabled
            onTriggered: root.triggerWebAction(WebEngineView.Reload)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Undo")
            enabled: root.action(WebEngineView.Undo).enabled
            onTriggered: root.triggerWebAction(WebEngineView.Undo)
        }
        MenuItem {
            text: qsTr("Redo")
            enabled: root.action(WebEngineView.Redo).enabled
            onTriggered: root.triggerWebAction(WebEngineView.Redo)
        }
        MenuItem {
            text: qsTr("Cut")
            enabled: root.action(WebEngineView.Cut).enabled
            onTriggered: root.triggerWebAction(WebEngineView.Cut)
        }
        MenuItem {
            text: qsTr("Copy")
            enabled: root.action(WebEngineView.Copy).enabled
            onTriggered: root.triggerWebAction(WebEngineView.Copy)
        }
        MenuItem {
            text: qsTr("Paste")
            enabled: root.action(WebEngineView.Paste).enabled
            onTriggered: root.triggerWebAction(WebEngineView.Paste)
        }
        MenuItem {
            text: qsTr("Paste and Match Style")
            enabled: root.action(WebEngineView.PasteAndMatchStyle).enabled
            onTriggered: root.triggerWebAction(WebEngineView.PasteAndMatchStyle)
        }
        MenuItem {
            text: qsTr("Select all")
            enabled: root.action(WebEngineView.SelectAll).enabled
            onTriggered: root.triggerWebAction(WebEngineView.SelectAll)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Copy link")
            visible: root.action(WebEngineView.CopyLinkToClipboard).enabled
            onTriggered: root.triggerWebAction(WebEngineView.CopyLinkToClipboard)
        }
        MenuItem {
            text: qsTr("Open link in default browser")
            visible: contextMenu.linkUrl.toString() !== ""
            onTriggered: root.navigationPolicy.openExternal(contextMenu.linkUrl)
        }
        MenuItem {
            text: qsTr("Download link")
            visible: root.action(WebEngineView.DownloadLinkToDisk).enabled
            onTriggered: root.triggerWebAction(WebEngineView.DownloadLinkToDisk)
        }
        MenuSeparator {
            visible: copyImage.visible || copyImageUrl.visible || downloadImage.visible
        }
        MenuItem {
            id: copyImage
            text: qsTr("Copy image")
            visible: root.action(WebEngineView.CopyImageToClipboard).enabled
            onTriggered: root.triggerWebAction(WebEngineView.CopyImageToClipboard)
        }
        MenuItem {
            id: copyImageUrl
            text: qsTr("Copy image address")
            visible: root.action(WebEngineView.CopyImageUrlToClipboard).enabled
            onTriggered: root.triggerWebAction(WebEngineView.CopyImageUrlToClipboard)
        }
        MenuItem {
            id: downloadImage
            text: qsTr("Download image")
            visible: root.action(WebEngineView.DownloadImageToDisk).enabled
            onTriggered: root.triggerWebAction(WebEngineView.DownloadImageToDisk)
        }
        MenuSeparator { visible: copyMediaUrl.visible || downloadMedia.visible }
        MenuItem {
            id: copyMediaUrl
            text: qsTr("Copy media address")
            visible: root.action(WebEngineView.CopyMediaUrlToClipboard).enabled
            onTriggered: root.triggerWebAction(WebEngineView.CopyMediaUrlToClipboard)
        }
        MenuItem {
            id: downloadMedia
            text: qsTr("Download media")
            visible: root.action(WebEngineView.DownloadMediaToDisk).enabled
            onTriggered: root.triggerWebAction(WebEngineView.DownloadMediaToDisk)
        }
        MenuItem {
            text: qsTr("Play / Pause")
            visible: root.action(WebEngineView.ToggleMediaPlayPause).enabled
            onTriggered: root.triggerWebAction(WebEngineView.ToggleMediaPlayPause)
        }
        MenuItem {
            text: qsTr("Mute / Unmute")
            visible: root.action(WebEngineView.ToggleMediaMute).enabled
            onTriggered: root.triggerWebAction(WebEngineView.ToggleMediaMute)
        }
        MenuItem {
            text: qsTr("Loop")
            visible: root.action(WebEngineView.ToggleMediaLoop).enabled
            onTriggered: root.triggerWebAction(WebEngineView.ToggleMediaLoop)
        }
        MenuItem {
            text: qsTr("Show / Hide Controls")
            visible: root.action(WebEngineView.ToggleMediaControls).enabled
            onTriggered: root.triggerWebAction(WebEngineView.ToggleMediaControls)
        }
    }

    onContextMenuRequested: function(request) {
        request.accepted = true
        contextMenu.linkUrl = request.linkUrl
        contextMenu.popup()
    }

    onAuthenticationDialogRequested: function(request) {
        requestDialogs.openAuthentication(request)
    }
    onPermissionRequested: function(request) {
        requestDialogs.openPermission(request)
    }
    onCertificateError: function(error) {
        requestDialogs.openCertificate(error)
    }
    onFileSystemAccessRequested: function(request) {
        requestDialogs.openFileSystemAccess(request)
    }
    onRegisterProtocolHandlerRequested: function(request) {
        requestDialogs.openProtocolHandler(request)
    }
    onSelectClientCertificate: function(selection) {
        if (selection.certificates.length > 0)
            selection.select(selection.certificates[0])
        else
            selection.selectNone()
    }
    onDesktopMediaRequested: function(request) {
        if (request.screensModel && request.screensModel.rowCount() > 0)
            request.selectScreen(request.screensModel.index(0, 0))
        else
            request.cancel()
    }
    onWebAuthUxRequested: function(request) {
        requestDialogs.openWebAuth(request)
    }

    onNewWindowRequested: function(request) {
        root.popupManager.open(request, root.url, root.webProfile,
                               root.Window.window)
    }

    onRenderProcessTerminated: function(terminationStatus, exitCode) {
        if (terminationStatus !== WebEngineView.NormalTerminationStatus)
            requestDialogs.openRendererRecovery(terminationStatus, exitCode)
    }
}
