// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import WebAppContainer

ApplicationWindow {
    id: root
    required property DownloadModel downloadModel
    width: 400
    height: 212
    minimumWidth: 320
    minimumHeight: 160
    title: qsTr("Downloads")

    background: Rectangle { color: root.palette.button }

    ListView {
        id: list
        property var downloads: root.downloadModel
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8
        model: root.downloadModel
        clip: true
        Rectangle { anchors.fill: parent; color: root.palette.mid; z: -1 }
        // qmllint disable unqualified
        delegate: DownloadDelegate { downloadModel: list.downloads }
        // qmllint enable unqualified
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOn }
    }

    Label {
        anchors.centerIn: parent
        visible: list.count === 0
        text: qsTr("No downloads")
        color: root.palette.placeholderText
    }
}
