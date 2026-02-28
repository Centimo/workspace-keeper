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
  property int cellSize: PlasmaCore.Units.iconSizes.small

  Layout.preferredWidth: cellSize
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
      property string state: status ? status.state : "not_running"

      Layout.preferredWidth: root.cellSize
      Layout.preferredHeight: root.cellSize
      radius: 3
      color: stateColor(state)
      border.color: Qt.lighter(color, 1.3)
      border.width: 1

      Text {
        anchors.centerIn: parent
        text: stateLabel(btn.state)
        color: stateTextColor(btn.state)
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

  // === Functions ===

  function stateColor(state) {
    switch (state) {
      case "working": return "#1d5fa0";
      case "waiting": return "#b08020";
      case "idle":    return "#555555";
      default:        return "#3a3a3a";
    }
  }

  function stateTextColor(state) {
    switch (state) {
      case "working": return "#ffffff";
      case "waiting": return "#ffffff";
      case "idle":    return "#aaaaaa";
      default:        return "#666666";
    }
  }

  function stateLabel(state) {
    switch (state) {
      case "working": return "W";
      case "waiting": return "?";
      case "idle":    return "_";
      default:        return "-";
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
