// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import WebAppContainer

QtObject {
    id: root
    required property NavigationPolicy navigationPolicy
    required property SecurityBridge securityBridge
    required property SettingsStore settingsStore
    required property string pushSubscriptionScript

    property Component popupComponent: Component { PopupWindow {} }

    function open(request, openerUrl, webProfile, parentWindow) {
        if (request.requestedUrl.toString() !== "" &&
                !root.navigationPolicy.shouldOpenInApplication(
                    openerUrl, request.requestedUrl)) {
            root.navigationPolicy.openExternal(request.requestedUrl)
            return
        }
        const popup = root.popupComponent.createObject(root, {
            "webProfile": webProfile,
            "navigationPolicy": root.navigationPolicy,
            "securityBridge": root.securityBridge,
            "settingsStore": root.settingsStore,
            "pushSubscriptionScript": root.pushSubscriptionScript,
            "popupManager": root,
            "transientParent": parentWindow,
            "requestedGeometry": request.requestedGeometry
        })
        if (popup === null)
            return
        // qmllint disable missing-property
        request.openIn(popup.webView)
        popup.show()
        // qmllint enable missing-property
    }
}
