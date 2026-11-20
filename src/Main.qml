import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 960
    height: 680
    minimumWidth: 640
    minimumHeight: 460
    visible: true
    title: "Vitals"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool helpVisible: false
    readonly property color secondColor: "#8a8a92"

    // These read translator.language, so every binding that formats a number
    // re-renders with the new separators when the Omarchy language changes.
    function number(value, digits) {
        return translator.language, value.toLocaleString(Qt.locale(), 'f', digits)
    }
    function percent(value) {
        return number(value, 0) + "%"
    }
    function bytes(value) {
        const units = ["B", "KiB", "MiB", "GiB", "TiB"]
        let i = 0
        while (value >= 1024 && i < units.length - 1) { value /= 1024; ++i }
        return number(value, i === 0 ? 0 : value < 10 ? 1 : 0) + " " + units[i]
    }
    function rate(value) {
        return bytes(value) + "/s"
    }
    function duration(seconds) {
        const d = Math.floor(seconds / 86400), h = Math.floor(seconds % 86400 / 3600), m = Math.floor(seconds % 3600 / 60)
        if (d > 0) return qsTr("%1d %2h").arg(number(d, 0)).arg(number(h, 0))
        if (h > 0) return qsTr("%1h %2m").arg(number(h, 0)).arg(number(m, 0))
        return qsTr("%1m").arg(number(m, 0))
    }

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Esc"; context: Qt.ApplicationShortcut; enabled: win.helpVisible; onActivated: win.helpVisible = false }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: backend.togglePause() }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Label {
                    text: "Vitals"
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    color: "#f2f2f4"
                }
                Rectangle {
                    visible: backend.paused
                    color: theme.accent
                    radius: Math.min(4, theme.radius)
                    implicitWidth: pausedLabel.implicitWidth + 16
                    implicitHeight: pausedLabel.implicitHeight + 6
                    Label {
                        id: pausedLabel
                        anchors.centerIn: parent
                        text: qsTr("Paused")
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: theme.accentForeground
                    }
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: qsTr("Up %1").arg(win.duration(backend.uptime))
                    color: "#a8a8b0"
                    font.pixelSize: 13
                }
                Label {
                    text: qsTr("Press ? for shortcuts")
                    color: "#5c5c64"
                    font.pixelSize: 13
                }
            }

            // A missing /proc file or unreadable disk is said out loud rather
            // than shown as a quiet zero.
            Label {
                visible: backend.error.length > 0
                text: backend.error
                color: "#ff6b6b"
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: 2
                rowSpacing: 16
                columnSpacing: 16
                opacity: backend.paused ? 0.75 : 1

                MetricCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("CPU")
                    value: win.percent(backend.cpu)
                    detail: qsTr("Load %1").arg(backend.load.map(v => win.number(v, 2)).join("  "))
                            + "  ·  " + qsTr("%n cores", "", backend.cores.length)
                    graph.values: backend.cpuHistory
                    graph.maximum: 100

                    RowLayout {
                        width: parent.width
                        spacing: 2
                        Repeater {
                            model: backend.cores
                            Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                implicitHeight: 14
                                color: "#232327"
                                radius: Math.min(2, theme.radius)
                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: parent.height * modelData / 100
                                    color: theme.accent
                                    radius: parent.radius
                                }
                            }
                        }
                    }
                }

                MetricCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Memory")
                    value: win.percent(backend.memTotal > 0 ? 100 * backend.memUsed / backend.memTotal : 0)
                    detail: qsTr("%1 of %2").arg(win.bytes(backend.memUsed)).arg(win.bytes(backend.memTotal))
                            + "  ·  " + (backend.swapTotal > 0
                                ? qsTr("Swap %1 of %2").arg(win.bytes(backend.swapUsed)).arg(win.bytes(backend.swapTotal))
                                : qsTr("No swap"))
                    graph.values: backend.memHistory
                    graph.maximum: 100
                }

                MetricCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Disk")
                    value: win.percent(backend.diskTotal > 0 ? 100 * backend.diskUsed / backend.diskTotal : 0)
                    detail: qsTr("%1 free of %2").arg(win.bytes(backend.diskTotal - backend.diskUsed))
                                                  .arg(win.bytes(backend.diskTotal))
                    graph.values: backend.diskReadHistory
                    graph.secondValues: backend.diskWriteHistory
                    graph.secondColor: win.secondColor

                    Legend {
                        width: parent.width
                        firstLabel: qsTr("Read %1").arg(win.rate(backend.diskRead))
                        secondLabel: qsTr("Write %1").arg(win.rate(backend.diskWrite))
                        secondColor: win.secondColor
                    }
                }

                MetricCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Network")
                    value: "↓ " + win.rate(backend.netRx)
                    detail: "↑ " + win.rate(backend.netTx)
                    graph.values: backend.rxHistory
                    graph.secondValues: backend.txHistory
                    graph.secondColor: win.secondColor

                    Legend {
                        width: parent.width
                        firstLabel: qsTr("Received")
                        secondLabel: qsTr("Sent")
                        secondColor: win.secondColor
                    }
                }
            }
        }

        ShortcutOverlay {
            anchors.fill: parent
            visible: win.helpVisible
            onClosed: win.helpVisible = false
        }
    }
}
