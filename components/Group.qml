import QtQuick 2.0
import Ubuntu.Components 0.1
import Ubuntu.Components.ListItems 0.1 as ListItem
import UQQ 1.0 as QQ

Item {
    id: root

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: wrapper.height

        Column {
            id: wrapper
            width: parent.width

            Repeater {
                id: groups

                model: QQ.Client.getGroupList()

                Category {
                    id: group

                    property bool loaded: false

                    width: parent.width
                    maxHeight: root.height
                    title: modelData.markname == "" ? modelData.name : modelData.markname
                    subtitle: (modelData.markname == "" ? "" : "(" + modelData.name + ")") +
                              (modelData.total > 0 ? " [" + modelData.online + "/" + modelData.total + "]" : "")
                    iconSource: "../group.png"
                    statusSource: modelData.messageMask !== QQ.Category.MessageNotify ? "../block.png" : ""
                    iconPageSource:"GroupMessage.qml"
                    messageCount: modelData.messageCount

                    onOpenedChanged: {
                        if (opened) {
                            //console.log("opened")
                            group.model = QQ.Client.getGroupMembers(modelData.id);
                        } else {
                            //console.log("closed")
                            group.model = 0;
                        }
                    }

                    Connections {
                        target: QQ.Client
                        onGroupReady: {
                            if (gid === modelData.id) {
                                group.model = QQ.Client.getGroupMembers(gid);
                                group.loaded = true;
                            }
                        }
                        onSessionMessageReceived: {
                            if (gid == modelData.id && group.state == "")
                                    group.newMsg = true;
                        }
                    }

                    onClicked: {
                        if (!loaded) {
                            QQ.Client.loadGroupInfo(modelData.id);
                        }
                    }
                    onIconClicked: {
                        if (!loaded) {
                            QQ.Client.loadGroupInfo(modelData.id);
                        }
                    }
                }
            }
        }
    }
}
