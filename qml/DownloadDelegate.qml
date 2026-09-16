// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root
    required property var downloadId
    required property string fileName
    required property real progress
    required property bool indeterminate
    required property bool canCancel
    required property bool canRemove
    required property string interruptionReason
    required property double receivedBytes
    required property double totalBytes
    required property double bytesPerSecond
    required property var downloadModel
    width: ListView.view ? ListView.view.width : implicitWidth

    function formatBytes(value) {
        if (value < 0)
            return qsTr("Unknown")
        const units = [qsTr("B"), qsTr("KiB"), qsTr("MiB"), qsTr("GiB")]
        let amount = value
        let unit = 0
        while (amount >= 1024 && unit < units.length - 1) {
            amount /= 1024
            ++unit
        }
        return (unit === 0 ? Math.round(amount) : amount.toFixed(1)) + " " + units[unit]
    }

    contentItem: ColumnLayout {
        spacing: 6
        Label {
            text: root.fileName
            Layout.fillWidth: true
            elide: Text.ElideMiddle
            font.bold: true
        }
        ProgressBar {
            from: 0
            to: 1
            value: root.progress
            indeterminate: root.indeterminate
            Layout.fillWidth: true
        }
        Label {
            Layout.fillWidth: true
            color: root.palette.placeholderText
            text: root.totalBytes > 0
                  ? qsTr("%1 of %2 — %3/s")
                    .arg(root.formatBytes(root.receivedBytes))
                    .arg(root.formatBytes(root.totalBytes))
                    .arg(root.formatBytes(root.bytesPerSecond))
                  : qsTr("%1 — %2/s")
                    .arg(root.formatBytes(root.receivedBytes))
                    .arg(root.formatBytes(root.bytesPerSecond))
        }
        Label {
            Layout.fillWidth: true
            text: root.interruptionReason
            visible: text.length > 0
            color: root.palette.brightText
            wrapMode: Text.Wrap
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            Button {
                text: qsTr("Cancel")
                visible: root.canCancel
                onClicked: root.downloadModel.cancel(root.downloadId)
            }
            Button {
                text: qsTr("Remove")
                visible: root.canRemove
                onClicked: root.downloadModel.remove(root.downloadId)
            }
        }
    }
}
