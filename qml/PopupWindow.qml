// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtWebEngine
import WebAppContainer

Window {
    id: root
    required property var webProfile
    required property NavigationPolicy navigationPolicy
    required property SecurityBridge securityBridge
    required property SettingsStore settingsStore
    required property string pushSubscriptionScript
    required property var popupManager
    property rect requestedGeometry
    property alias webView: view

    width: 400
    height: 600
    title: view.title
    onClosing: root.destroy()

    function applyRequestedGeometry(geometry) {
        if (geometry && geometry.width > 30 && geometry.height > 30) {
            root.x = geometry.x
            root.y = geometry.y
            root.width = geometry.width
            root.height = geometry.height
        } else {
            const available = root.navigationPolicy.availableGeometry(
                                  root.transientParent)
            if (available.width <= 0 || available.height <= 0)
                return
            root.x = available.x + Math.round((available.width - root.width) / 2)
            root.y = available.y + Math.round((available.height - root.height) / 2)
        }
    }

    Component.onCompleted: {
        root.applyRequestedGeometry(root.requestedGeometry)
        view.forceActiveFocus()
    }

    WebEngineView {
        id: view
        anchors.fill: parent
        profile: root.webProfile
        settings.javascriptEnabled: true
        settings.localStorageEnabled: true
        settings.pluginsEnabled: true
        settings.dnsPrefetchEnabled: true
        settings.localContentCanAccessRemoteUrls: true
        settings.localContentCanAccessFileUrls: false
        settings.screenCaptureEnabled: true
        // qmllint disable unqualified unresolved-type
        userScripts.collection: [{
            name: "WebAppContainer Push Subscription Persistence",
            sourceCode: root.pushSubscriptionScript,
            injectionPoint: WebEngineScript.DocumentCreation,
            worldId: WebEngineScript.MainWorld,
            runsOnSubFrames: false
        }, {
            name: "WebAppContainer Media Permission Bootstrap",
            sourceCode: root.settingsStore.mediaPermissionBootstrapScript,
            injectionPoint: WebEngineScript.DocumentCreation,
            worldId: WebEngineScript.MainWorld,
            runsOnSubFrames: false
        }]
        // qmllint enable unqualified unresolved-type
        onNewWindowRequested: function(request) {
            root.popupManager.open(request, view.url, root.webProfile, root)
        }
        onGeometryChangeRequested: function(geometry, frameGeometry) {
            root.applyRequestedGeometry(geometry)
            root.show()
            view.forceActiveFocus()
        }
        RequestDialogs {
            id: popupDialogs
            anchors.fill: parent
            securityBridge: root.securityBridge
            settingsStore: root.settingsStore
            onReloadRequested: view.reload()
        }
        onAuthenticationDialogRequested: function(request) {
            popupDialogs.openAuthentication(request)
        }
        onPermissionRequested: function(request) {
            popupDialogs.openPermission(request)
        }
        onCertificateError: function(error) {
            popupDialogs.openCertificate(error)
        }
        onFileSystemAccessRequested: function(request) {
            popupDialogs.openFileSystemAccess(request)
        }
        onRegisterProtocolHandlerRequested: function(request) {
            popupDialogs.openProtocolHandler(request)
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
            popupDialogs.openWebAuth(request)
        }
        onRenderProcessTerminated: function(status, exitCode) {
            if (status !== WebEngineView.NormalTerminationStatus)
                popupDialogs.openRendererRecovery(status, exitCode)
        }
        onWindowCloseRequested: root.close()
    }
}
