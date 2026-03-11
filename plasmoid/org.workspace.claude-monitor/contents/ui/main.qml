import QtQuick 2.15
import QtQuick.Layouts 1.15
import org.kde.plasma.plasmoid 2.0
import org.kde.plasma.core 2.0 as PlasmaCore
import org.workspace.monitor 1.0

ColumnLayout {
  id: root

  spacing: 2

  Plasmoid.preferredRepresentation: Plasmoid.fullRepresentation

  property var workspaces: []
  property var claudeStatuses: ({})
  property int cellSize: width > 0 ? width : PlasmaCore.Units.iconSizes.small

  Layout.fillWidth: true
  Layout.minimumWidth: cellSize
  Layout.preferredHeight: workspaces.length > 0
    ? workspaces.length * (cellSize + spacing) - spacing
    : cellSize
  Layout.minimumHeight: cellSize

  WorkspaceMonitor {
    id: monitor
    onDesktopsChanged: root.workspaces = monitor.desktops.map(function(d) { return d.name; })
    onClaudeStatusesChanged: root.claudeStatuses = monitor.claudeStatuses
  }

  Repeater {
    model: root.workspaces

    Rectangle {
      id: btn

      required property int index
      required property string modelData
      property string workspaceName: modelData
      property var status: root.claudeStatuses[workspaceName] || null
      property string stateStr: status ? status.state : ""

      Layout.fillWidth: true
      Layout.preferredHeight: width
      radius: 3
      color: monitor.stateColor(stateStr)
      border.color: Qt.lighter(color, 1.3)
      border.width: 1

      Text {
        anchors.centerIn: parent
        text: monitor.stateLabel(btn.stateStr)
        color: monitor.stateTextColor(btn.stateStr)
        font.family: "Hack, monospace"
        font.pixelSize: Math.round(root.cellSize * 0.4)
        font.bold: true
      }

      MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: monitor.switchToDesktop(btn.index)
      }

      PlasmaCore.ToolTipArea {
        anchors.fill: parent
        mainText: btn.workspaceName
        subText: tooltipText(btn.status)
      }
    }
  }

  function tooltipText(status) {
    if (!status) return "Claude not running";
    var text = "State: " + status.state;
    if (status.tool_name) text += "\nTool: " + status.tool_name;
    if (status.wait_reason) text += "\nWaiting for: " + status.wait_reason;
    if (status.wait_message) text += "\nMessage: " + status.wait_message;
    if (status.state_since_ms) {
      var elapsed = Math.floor((Date.now() - status.state_since_ms) / 1000);
      if (elapsed < 60) text += "\nDuration: " + elapsed + "s";
      else text += "\nDuration: " + Math.floor(elapsed / 60) + "m " + (elapsed % 60) + "s";
    }
    return text;
  }
}
