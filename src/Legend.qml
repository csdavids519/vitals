import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Names the two lines on a graph, in the colors they're drawn with.
RowLayout {
    property string firstLabel
    property string secondLabel
    property color secondColor

    spacing: 16

    Repeater {
        model: [
            { label: firstLabel, color: theme.accent },
            { label: secondLabel, color: secondColor }
        ]
        RowLayout {
            required property var modelData
            spacing: 6
            Layout.fillWidth: true
            Rectangle { implicitWidth: 8; implicitHeight: 8; radius: 4; color: modelData.color }
            Label {
                text: modelData.label
                font.pixelSize: 12
                color: "#a8a8b0"
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }
}
