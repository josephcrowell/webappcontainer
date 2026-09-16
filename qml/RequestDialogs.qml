// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import QtWebEngine
import WebAppContainer

Item {
    id: root
    required property SecurityBridge securityBridge
    required property SettingsStore settingsStore
    property var authenticationRequest: null
    property var permissionRequest: null
    property var certificateRequest: null
    property var fileSystemRequest: null
    property var protocolRequest: null
    property var webAuthRequest: null
    signal reloadRequested()

    function openAuthentication(request) {
        request.accepted = true
        root.authenticationRequest = request
        userName.text = ""
        password.text = ""
        authenticationDialog.open()
        userName.forceActiveFocus()
    }

    function openPermission(request) {
        if (root.settingsStore.hasPermissionGrant(request.origin,
                                                  request.permissionType)) {
            request.grant()
            return
        }
        root.permissionRequest = request
        permissionDialog.open()
    }

    function openCertificate(error) {
        if (!root.securityBridge.certificateIsMainFrame(error) || !error.overridable) {
            error.rejectCertificate()
            return
        }
        error.defer()
        root.certificateRequest = error
        certificateDialog.open()
    }

    property string rendererFailureText: ""

    function openRendererRecovery(status, exitCode) {
        let description = qsTr("Render process exited")
        if (status === WebEngineView.AbnormalTerminationStatus)
            description = qsTr("Render process abnormal exit")
        else if (status === WebEngineView.CrashedTerminationStatus)
            description = qsTr("Render process crashed")
        else if (status === WebEngineView.KilledTerminationStatus)
            description = qsTr("Render process killed")
        root.rendererFailureText = qsTr("%1 with code: %2\nDo you want to reload the page?")
                                   .arg(description).arg(exitCode)
        rendererDialog.open()
    }

    function openFileSystemAccess(request) {
        root.fileSystemRequest = request
        fileSystemDialog.open()
    }

    function openProtocolHandler(request) {
        const decision = root.settingsStore.protocolHandlerDecision(request.origin,
                                                                    request.scheme)
        if (decision === 1) {
            request.accept()
            return
        }
        if (decision === 0) {
            request.reject()
            return
        }
        root.protocolRequest = request
        protocolDialog.open()
    }

    function openWebAuth(request) {
        root.webAuthRequest = request
        authPin.text = ""
        authPinConfirm.text = ""
        webAuthDialog.open()
    }

    Dialog {
        id: authenticationDialog
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.CloseOnEscape
        title: root.authenticationRequest && root.authenticationRequest.proxy
               ? qsTr("Proxy authentication") : qsTr("Authentication required")
        onClosed: {
            if (root.authenticationRequest)
                root.authenticationRequest.dialogReject()
            root.authenticationRequest = null
            password.text = ""
        }
        contentItem: Column {
            spacing: 8
            Label {
                text: !root.authenticationRequest ? ""
                      : root.authenticationRequest.proxy
                        ? qsTr("Connect to proxy \"%1\" using:")
                          .arg(root.authenticationRequest.host)
                        : qsTr("Enter username and password for \"%1\" at %2")
                          .arg(root.authenticationRequest.realm)
                          .arg(root.authenticationRequest.host)
                wrapMode: Text.Wrap
            }
            TextField { id: userName; placeholderText: qsTr("User name") }
            TextField {
                id: password
                placeholderText: qsTr("Password")
                echoMode: TextInput.Password
                onAccepted: authButtons.accepted()
            }
        }
        footer: DialogButtonBox {
            id: authButtons
            standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
            defaultStandardButton: DialogButtonBox.Cancel
            onAccepted: {
                if (!root.authenticationRequest)
                    return
                const request = root.authenticationRequest
                root.authenticationRequest = null
                request.dialogAccept(userName.text, password.text)
                password.text = ""
                authenticationDialog.close()
            }
            onRejected: authenticationDialog.close()
        }
    }

    Dialog {
        id: permissionDialog
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.CloseOnEscape
        title: qsTr("Permission request")
        contentItem: Label {
            text: root.permissionRequest
                  ? root.securityBridge.permissionQuestion(root.permissionRequest)
                  : ""
            wrapMode: Text.Wrap
        }
        onClosed: {
            if (root.permissionRequest)
                root.permissionRequest.deny()
            root.permissionRequest = null
        }
        footer: DialogButtonBox {
            standardButtons: DialogButtonBox.Yes | DialogButtonBox.No
            defaultStandardButton: DialogButtonBox.No
            onAccepted: {
                if (!root.permissionRequest)
                    return
                const request = root.permissionRequest
                root.permissionRequest = null
                root.settingsStore.rememberPermissionGrant(request.origin,
                                                           request.permissionType)
                request.grant()
                permissionDialog.close()
            }
            onRejected: permissionDialog.close()
        }
    }

    Dialog {
        id: certificateDialog
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.CloseOnEscape
        title: qsTr("Certificate error")
        contentItem: Label {
            text: root.certificateRequest ? root.certificateRequest.description : ""
            wrapMode: Text.Wrap
        }
        onClosed: {
            if (root.certificateRequest)
                root.certificateRequest.rejectCertificate()
            root.certificateRequest = null
        }
        footer: DialogButtonBox {
            standardButtons: DialogButtonBox.Yes | DialogButtonBox.No
            defaultStandardButton: DialogButtonBox.No
            onAccepted: {
                if (!root.certificateRequest)
                    return
                const request = root.certificateRequest
                root.certificateRequest = null
                request.acceptCertificate()
                certificateDialog.close()
            }
            onRejected: certificateDialog.close()
        }
    }

    Dialog {
        id: webAuthDialog
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.CloseOnEscape
        title: qsTr("Passkey request")
        property bool collectingPin: root.webAuthRequest !== null &&
                                     root.webAuthRequest.state === WebEngineWebAuthUxRequest.WebAuthUxState.CollectPin
        property bool selectingAccount: root.webAuthRequest !== null &&
                                          root.webAuthRequest.state === WebEngineWebAuthUxRequest.WebAuthUxState.SelectAccount
        property bool confirmPin: collectingPin &&
                                  root.webAuthRequest.pinRequest.reason !==
                                  WebEngineWebAuthUxRequest.PinEntryReason.Challenge
        function submit() {
            if (!root.webAuthRequest)
                return
            if (selectingAccount) {
                if (accountBox.currentText !== "")
                    root.webAuthRequest.setSelectedAccount(accountBox.currentText)
                return
            }
            if (collectingPin && authPin.text.length >= root.webAuthRequest.pinRequest.minPinLength &&
                    (!confirmPin || authPin.text === authPinConfirm.text)) {
                root.webAuthRequest.setPin(authPin.text)
                authPin.text = ""
                authPinConfirm.text = ""
            }
        }
        onClosed: {
            if (root.webAuthRequest &&
                    root.webAuthRequest.state !== WebEngineWebAuthUxRequest.WebAuthUxState.Completed &&
                    root.webAuthRequest.state !== WebEngineWebAuthUxRequest.WebAuthUxState.Cancelled)
                root.webAuthRequest.cancel()
            root.webAuthRequest = null
            authPin.text = ""
            authPinConfirm.text = ""
        }
        contentItem: Column {
            spacing: 8
            Label {
                text: root.webAuthRequest
                      ? qsTr("Request from %1").arg(root.webAuthRequest.relyingPartyId)
                      : ""
            }
            Label {
                visible: webAuthDialog.collectingPin && text.length > 0
                text: root.securityBridge.webAuthPinError(root.webAuthRequest)
                color: webAuthDialog.palette.brightText
                wrapMode: Text.Wrap
            }
            ComboBox {
                id: accountBox
                visible: webAuthDialog.selectingAccount
                model: root.webAuthRequest ? root.webAuthRequest.userNames : []
            }
            Label {
                visible: webAuthDialog.collectingPin && root.webAuthRequest !== null
                text: root.webAuthRequest
                      ? qsTr("PIN must contain at least %1 characters. Attempts remaining: %2")
                        .arg(root.webAuthRequest.pinRequest.minPinLength)
                        .arg(root.webAuthRequest.pinRequest.remainingAttempts)
                      : ""
                wrapMode: Text.Wrap
            }
            TextField {
                id: authPin
                visible: webAuthDialog.collectingPin
                placeholderText: qsTr("PIN")
                echoMode: TextInput.Password
                onAccepted: webAuthDialog.submit()
            }
            TextField {
                id: authPinConfirm
                visible: webAuthDialog.confirmPin
                placeholderText: qsTr("Confirm PIN")
                echoMode: TextInput.Password
                onAccepted: webAuthDialog.submit()
            }
            Label {
                visible: root.webAuthRequest !== null &&
                         root.webAuthRequest.state === WebEngineWebAuthUxRequest.WebAuthUxState.FinishTokenCollection
                text: qsTr("Complete the request on your security key.")
                wrapMode: Text.Wrap
            }
            Label {
                visible: root.webAuthRequest !== null &&
                         root.webAuthRequest.state === WebEngineWebAuthUxRequest.WebAuthUxState.RequestFailed
                text: root.securityBridge.webAuthFailure(root.webAuthRequest)
                wrapMode: Text.Wrap
            }
            Row {
                spacing: 8
                Button {
                    text: qsTr("Continue")
                    visible: webAuthDialog.selectingAccount || webAuthDialog.collectingPin
                    enabled: webAuthDialog.selectingAccount
                             ? accountBox.currentText !== ""
                             : authPin.text.length >= (root.webAuthRequest
                                   ? root.webAuthRequest.pinRequest.minPinLength : 1) &&
                               (!webAuthDialog.confirmPin || authPin.text === authPinConfirm.text)
                    onClicked: webAuthDialog.submit()
                }
                Button {
                    text: qsTr("Retry")
                    visible: root.webAuthRequest !== null &&
                             root.webAuthRequest.state === WebEngineWebAuthUxRequest.WebAuthUxState.RequestFailed &&
                             root.securityBridge.webAuthCanRetry(root.webAuthRequest)
                    onClicked: root.webAuthRequest.retry()
                }
                Button { text: qsTr("Cancel"); onClicked: webAuthDialog.close() }
            }
        }
        Connections {
            target: root.webAuthRequest
            function onStateChanged(state) {
                if (state === WebEngineWebAuthUxRequest.WebAuthUxState.Completed ||
                        state === WebEngineWebAuthUxRequest.WebAuthUxState.Cancelled)
                    webAuthDialog.close()
                else if (state === WebEngineWebAuthUxRequest.WebAuthUxState.CollectPin)
                    authPin.forceActiveFocus()
            }
        }
    }

    Dialog {
        id: protocolDialog
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.CloseOnEscape
        title: qsTr("Protocol handler")
        contentItem: Label {
            text: root.protocolRequest
                  ? qsTr("Allow %1 to handle %2 links?")
                        .arg(root.protocolRequest.origin)
                        .arg(root.protocolRequest.scheme)
                  : ""
            wrapMode: Text.Wrap
        }
        onClosed: {
            if (root.protocolRequest) {
                root.settingsStore.rememberProtocolHandlerDecision(
                    root.protocolRequest.origin, root.protocolRequest.scheme, false)
                root.protocolRequest.reject()
            }
            root.protocolRequest = null
        }
        footer: DialogButtonBox {
            standardButtons: DialogButtonBox.Yes | DialogButtonBox.No
            defaultStandardButton: DialogButtonBox.No
            onAccepted: {
                if (!root.protocolRequest)
                    return
                const request = root.protocolRequest
                root.protocolRequest = null
                root.settingsStore.rememberProtocolHandlerDecision(
                    request.origin, request.scheme, true)
                request.accept()
                protocolDialog.close()
            }
            onRejected: protocolDialog.close()
        }
    }

    Dialog {
        id: fileSystemDialog
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.CloseOnEscape
        title: qsTr("File system access")
        contentItem: Label {
            text: root.fileSystemRequest
                  ? root.securityBridge.fileSystemAccessQuestion(root.fileSystemRequest)
                  : ""
            wrapMode: Text.Wrap
        }
        onClosed: {
            if (root.fileSystemRequest)
                root.fileSystemRequest.reject()
            root.fileSystemRequest = null
        }
        footer: DialogButtonBox {
            standardButtons: DialogButtonBox.Yes | DialogButtonBox.No
            defaultStandardButton: DialogButtonBox.No
            onAccepted: {
                if (!root.fileSystemRequest)
                    return
                const request = root.fileSystemRequest
                root.fileSystemRequest = null
                request.accept()
                fileSystemDialog.close()
            }
            onRejected: fileSystemDialog.close()
        }
    }

    Dialog {
        id: rendererDialog
        anchors.centerIn: parent
        modal: true
        title: qsTr("Web page stopped")
        contentItem: Label { text: root.rendererFailureText; wrapMode: Text.Wrap }
        footer: DialogButtonBox {
            standardButtons: DialogButtonBox.Yes | DialogButtonBox.No
            defaultStandardButton: DialogButtonBox.No
            onAccepted: {
                rendererDialog.close()
                root.reloadRequested()
            }
            onRejected: rendererDialog.close()
        }
    }
}
