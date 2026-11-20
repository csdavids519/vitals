import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// One panel: what it measures, the number now, a line of detail, the graph,
// and room underneath for anything extra (the CPU's per-core bars).
Rectangle {
    id: card
    property string title
    property string value
    property string detail
    property alias graph: graph
    default property alias extra: extraSlot.data

    color: "#17171a"
    border.color: "#232327"
    radius: theme.radius
    Material.roundedScale: theme.radius

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 6

        Label {
            text: card.title
            font.pixelSize: 13
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 1.2
            color: "#8a8a92"
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        Label {
            text: card.value
            font.pixelSize: 30
            font.weight: Font.Bold
            color: "#f2f2f4"
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        Label {
            text: card.detail
            font.pixelSize: 13
            color: "#a8a8b0"
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        UsageGraph {
            id: graph
            color: theme.accent
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 6
            Layout.minimumHeight: 40
        }
        Item {
            id: extraSlot
            Layout.topMargin: 8
            Layout.fillWidth: true
            implicitHeight: childrenRect.height
            visible: children.length > 0
        }
    }
}
