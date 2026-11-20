import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Every key the app answers to, shown with ?.
Rectangle {
    id: overlay
    signal closed()
    color: Qt.rgba(0, 0, 0, 0.6)

    MouseArea { anchors.fill: parent; onClicked: overlay.closed() }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 420)
        height: content.implicitHeight + 48
        color: "#17171a"
        border.color: "#2c2c31"
        radius: theme.radius
        Material.roundedScale: theme.radius

        MouseArea { anchors.fill: parent }

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 24
            spacing: 12

            Label {
                text: qsTr("Keyboard shortcuts")
                font.pixelSize: 17
                font.weight: Font.Bold
                color: "#f2f2f4"
            }

            GridLayout {
                columns: 2
                columnSpacing: 20
                rowSpacing: 10
                Layout.fillWidth: true

                Repeater {
                    model: ["Space", "?", "Esc", "Q"]
                    delegate: Item {
                        required property var modelData
                        required property int index
                        Layout.row: index
                        Layout.column: 0
                        implicitWidth: keyCap.implicitWidth
                        implicitHeight: keyCap.implicitHeight
                        Rectangle {
                            id: keyCap
                            implicitWidth: Math.max(28, keyLabel.implicitWidth + 14)
                            implicitHeight: keyLabel.implicitHeight + 6
                            color: "#232327"
                            radius: Math.min(4, theme.radius)
                            Label {
                                id: keyLabel
                                anchors.centerIn: parent
                                text: modelData
                                font.family: "monospace"
                                font.pixelSize: 13
                                color: theme.accent
                            }
                        }
                    }
                }
                Repeater {
                    model: [qsTr("Pause or resume"), qsTr("Show or hide shortcuts"), qsTr("Close"), qsTr("Quit")]
                    delegate: Label {
                        required property var modelData
                        required property int index
                        Layout.row: index
                        Layout.column: 1
                        Layout.fillWidth: true
                        text: modelData
                        color: "#d6d6dc"
                        font.pixelSize: 14
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }
}
