import QtQuick 2.10 // Qt 5.10
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.0
import QtQuick.Layouts 1.1
import Quotient 1.0

Rectangle {
    id: root

    TimelineSettings {
        id: settings
        readonly property bool use_shuttle_dial: value("UI/use_shuttle_dial", true)

        Component.onCompleted: console.log("Using timeline font: " + font)
    }
    SystemPalette { id: defaultPalette; colorGroup: SystemPalette.Active }
    SystemPalette { id: disabledPalette; colorGroup: SystemPalette.Disabled }

    color: defaultPalette.base

    function humanSize(bytes)
    {
        if (!bytes)
            return qsTr("Unknown", "Unknown attachment size")
        if (bytes < 4000)
            return qsTr("%Ln byte(s)", "", bytes)
        bytes = Math.round(bytes / 100) / 10
        if (bytes < 2000)
            return qsTr("%L1 KB").arg(bytes)
        bytes = Math.round(bytes / 100) / 10
        if (bytes < 2000)
            return qsTr("%L1 MB").arg(bytes)
        return qsTr("%L1 GB").arg(Math.round(bytes / 100) / 10)
    }

    function mixColors(baseColor, mixedColor, mixRatio)
    {
        return Qt.tint(baseColor,
                Qt.rgba(mixedColor.r, mixedColor.g, mixedColor.b, mixRatio))
    }

    Rectangle {
        id: roomHeader

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: headerText.height + 5

        color: defaultPalette.window
        border.color: disabledPalette.windowText
        visible: room

        Image {
            id: roomAvatar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 2
            height: headerText.height

            source: room && room.avatarMediaId
                    ? "image://mtx/" + room.avatarMediaId : ""
            sourceSize { // Kinda safe upper limits
                height: root.height / 3
                width: parent.width
            }
            fillMode: Image.PreserveAspectFit

            AnimationBehavior on width {
                NormalNumberAnimation { easing.type: Easing.OutQuad }
            }
        }
        Binding {
            target: roomAvatar
            property: "width"
            value: roomHeader.width ? roomHeader.width / 2 : 0
            when: roomAvatar.width && roomHeader.width
                  && roomAvatar.width > roomHeader.width / 2
        }

        Column {
            id: headerText
            anchors.left: roomAvatar.right
            anchors.right: versionActionButton.left
            anchors.top: parent.top
            anchors.margins: 2

            spacing: 2

            TextEdit {
                id: roomName
                width: parent.width

                readonly property bool hasName: room && room.displayName !== ""
                text: hasName ? room.displayName : qsTr("(no name)")
                color: (hasName ? defaultPalette : disabledPalette).windowText
                ToolTipArea { text: parent.hasName ? room.htmlSafeName : "" }

                font.bold: true
                font.family: settings.font.family
                font.pointSize: settings.font.pointSize
                renderType: settings.render_type
                readOnly: true
                selectByKeyboard: true
                selectByMouse: true
            }
            Label {
                id: versionNotice
                visible: room && (room.isUnstable || room.successorId !== "")
                width: parent.width

                text: !room ? "" :
                    room.successorId !== ""
                              ? qsTr("This room has been upgraded.") :
                    room.isUnstable ? qsTr("Unstable room version!") : ""
                font.italic: true
                font.family: settings.font.family
                font.pointSize: settings.font.pointSize
                renderType: settings.render_type
                ToolTipArea { text: parent.text }
            }

            ScrollView {
                id: topicField
                width: parent.width
                height: Math.min(topicText.contentHeight,
                                 room ? root.height / 5 : 0)

                horizontalScrollBarPolicy: Qt.ScrollBarAlwaysOff
                verticalScrollBarPolicy: Qt.ScrollBarAsNeeded
                style: ScrollViewStyle { transientScrollBars: true }

                AnimationBehavior on height {
                    NormalNumberAnimation { easing.type: Easing.OutQuad }
                }

                // FIXME: The below TextEdit+MouseArea is a massive copy-paste
                // from TimelineItem.qml. We need to make a separate component
                // for these (RichTextField?).
                TextEdit {
                    id: topicText
                    width: topicField.width

                    readonly property bool hasTopic: room && room.topic !== ""
                    text: hasTopic
                          ? room.prettyPrint(room.topic) : qsTr("(no topic)")
                    color:
                        (hasTopic ? defaultPalette : disabledPalette).windowText
                    textFormat: TextEdit.RichText
                    font: settings.font
                    renderType: settings.render_type
                    readOnly: true
                    selectByKeyboard: true;
                    selectByMouse: true;
                    wrapMode: TextEdit.Wrap

                    onHoveredLinkChanged:
                        controller.showStatusMessage(hoveredLink)

                    onLinkActivated: controller.resourceRequested(link)
                }
            }
        }
        MouseArea {
            anchors.fill: headerText
            acceptedButtons: Qt.MiddleButton
            cursorShape: topicText.hoveredLink
                         ? Qt.PointingHandCursor : Qt.IBeamCursor

            onClicked: {
                if (topicText.hoveredLink)
                    controller.resourceRequested(topicText.hoveredLink,
                                                 "_interactive")
            }
        }
        ToolButton {
            id: versionActionButton
            visible: room && ((room.isUnstable && room.canSwitchVersions())
                              || room.successorId !== "")
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            width: visible * implicitWidth
            text: !room ? "" : room.successorId !== ""
                                ? qsTr("Go to\nnew room") : qsTr("Room\nsettings")

            onClicked:
                if (room.successorId !== "")
                    controller.resourceRequested(room.successorId, "join")
                else
                    controller.roomSettingsRequested()
        }
    }

    ScrollView {
        id: chatScrollView
        anchors.top: roomHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: if (settings.use_shuttle_dial) { shuttleDial.width }
        horizontalScrollBarPolicy: Qt.ScrollBarAlwaysOff
        verticalScrollBarPolicy: settings.use_shuttle_dial
                                 ? Qt.ScrollBarAlwaysOff : Qt.ScrollBarAlwaysOn
        style: ScrollViewStyle { transientScrollBars: true }

        DropArea {
            anchors.fill: parent
            onEntered: if (!room) { drag.accepted = false }
            onDropped: {
                if (drop.hasUrls) {
                    controller.fileDrop(drop.urls)
                    drop.acceptProposedAction()
                } else if (drop.hasText) {
                    controller.textDrop(drop.text)
                    drop.acceptProposedAction()
                }
            }
        }

        // This covers the area above a short chatView.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onReleased: controller.focusInput()
        }

        ListView {
            id: chatView

            model: messageModel
            delegate: TimelineItem {
                width: chatView.width
                view: chatView
                moving: chatView.moving || shuttleDial.value
            }
            verticalLayoutDirection: ListView.BottomToTop
            flickableDirection: Flickable.VerticalFlick
            flickDeceleration: 8000
            boundsMovement: Flickable.StopAtBounds
    //        pixelAligned: true
            cacheBuffer: 200

            section.property: "section"

            readonly property bool atBeginning: contentY + height == 0
            readonly property int largestVisibleIndex: count > 0 ?
                atBeginning ? 0 : indexAt(contentX, contentY + height - 1) : -1
            readonly property bool noNeedMoreContent:
                !room || room.eventsHistoryJob || room.allHistoryLoaded

            /// The number of events per height unit - always positive
            readonly property real eventDensity:
                contentHeight > 0 && count > 0 ? count / contentHeight : 0.03
                // 0.03 is just an arbitrary reasonable number

            property int lastRequestedEvents: 0
            property var textEditWithSelection

            function ensurePreviousContent() {
                if (noNeedMoreContent)
                    return

                // Take the current speed, or assume we can scroll 8 screens/s
                var velocity = moving ? -verticalVelocity : height * 8
                // Check if we're about to bump into the ceiling in
                // 2 seconds and if yes, request the amount of messages
                // enough to scroll at this rate for 3 more seconds
                if (velocity > 0 && contentY - velocity*2 < originY) {
                    lastRequestedEvents = velocity * eventDensity * 3
                    room.getPreviousContent(lastRequestedEvents)
                }
            }
            onContentYChanged: ensurePreviousContent()
            onContentHeightChanged: ensurePreviousContent()

            function saveViewport() {
                room.saveViewport(indexAt(contentX, contentY),
                                  largestVisibleIndex)
            }

            function onModelReset() {
                console.log("Model timeline reset")
                if (room)
                {
                    var lastScrollPosition = room.savedTopVisibleIndex()
                    if (lastScrollPosition === 0)
                        positionViewAtBeginning()
                    else
                    {
                        console.log("Scrolling to position", lastScrollPosition)
                        positionViewAtIndex(lastScrollPosition, ListView.Contain)
                    }
                }
            }

            function scrollUp(dy) {
                contentY -= dy
            }
            function scrollDown(dy) {
                contentY += dy
            }

            function onWheel(wheel) {
                if (wheel.angleDelta.x === 0) {
                    var yDelta = wheel.angleDelta.y * 10 / 36

                    if (yDelta > 0)
                        scrollUp(yDelta)
                    else
                        scrollDown(-yDelta)
                    wheel.accepted = true
                } else {
                    wheel.accepted = false
                }
            }
            Connections {
                target: controller
                onPageUpPressed:
                    chatView.scrollUp(chatView.height
                                      - sectionBanner.childrenRect.height)

                onPageDownPressed:
                    chatView.scrollDown(chatView.height
                                        - sectionBanner.childrenRect.height)
                onScrollViewTo: chatView.positionViewAtIndex(currentIndex, ListView.Contain)
            }

            Component.onCompleted: {
                console.log("QML view loaded")
                // FIXME: This is not on the right place: ListView may or
                // may not have updated his structures according to the new
                // model by now
                model.modelReset.connect(onModelReset)
            }

            onMovementEnded: saveViewport()

            populate: AnimatedTransition {
                FastNumberAnimation { property: "opacity"; from: 0; to: 1 }
            }

            add: AnimatedTransition {
                FastNumberAnimation { property: "opacity"; from: 0; to: 1 }
            }

            move: AnimatedTransition {
                FastNumberAnimation { property: "y"; }
                FastNumberAnimation { property: "opacity"; to: 1 }
            }

            displaced: AnimatedTransition {
                FastNumberAnimation {
                    property: "y";
                    easing.type: Easing.OutQuad
                }
                FastNumberAnimation { property: "opacity"; to: 1 }
            }

            Behavior on contentY {
                enabled: !chatView.moving && settings.enable_animations
                SmoothedAnimation {
                    id: scrollAnimation
                    // It would mislead the benchmark below
                    duration: settings.animations_duration_ms_impl > 0 ? settings.fast_animations_duration_ms / 4 : 0
                    maximumEasingTime: settings.animations_duration_ms_impl > 0 ? settings.fast_animations_duration_ms / 2 : 0

                    onRunningChanged: {
                        if (!running) {
                            chatView.saveViewport()
                            if (settings.animations_duration_ms_impl == 0)
                                console.timeEnd("scroll")
                        } else {
                            if (settings.animations_duration_ms_impl == 0)
                                console.time("scroll")
                        }
                    }
            }}
            // itemAt is a function, not a property so is not bound to new items
            // showing up underneath; contentHeight is used for that instead.
            readonly property var underlayingItem: contentHeight >= height &&
                itemAt(contentX, contentY + sectionBanner.height - 2)
            readonly property bool sectionBannerVisible: underlayingItem &&
                (!underlayingItem.sectionVisible || underlayingItem.y < contentY)

            Rectangle {
                id: sectionBanner
                z: 3 // On top of ListView sections that have z=2
                anchors.left: parent.left
                anchors.top: parent.top
                width: childrenRect.width + 2
                height: childrenRect.height + 2
                visible: chatView.sectionBannerVisible
                color: defaultPalette.window
                opacity: 0.9
                Label {
                    font.bold: true
                    font.family: settings.font.family
                    font.pointSize: settings.font.pointSize
                    color: disabledPalette.text
                    renderType: settings.render_type
                    text: chatView.underlayingItem ?
                              chatView.underlayingItem.ListView.section : ""
                }
            }
        }
    }
    Slider {
        id: shuttleDial
        orientation: Qt.Vertical
        height: chatScrollView.height
        anchors.right: parent.right
        anchors.verticalCenter: chatScrollView.verticalCenter
        enabled: settings.use_shuttle_dial
        visible: enabled

        style: SliderStyle {
            // Width and height are swapped below because SliderStyle assumes
            // a horizontal slider
            groove: Rectangle {
                color: defaultPalette.window
                border.color: defaultPalette.midlight
                implicitHeight: 8
                clip: true

                readonly property int requestedHistoryEventsCount:
                    room && room.eventsHistoryJob
                    ? chatView.lastRequestedEvents : 0
                readonly property real averageEvtHeight:
                    chatView.count == 0 && requestedHistoryEventsCount == 0 ? 0
                    : chatView.height
                      / (chatView.count + requestedHistoryEventsCount)

                Rectangle {
                    id: loadingEventsBar
                    // Loading history events bar, stacked above
                    // the cached events bar when more history is being loaded
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: cachedEventsBar.right
                    implicitHeight: 2
                    width: averageEvtHeight * requestedHistoryEventsCount

                    opacity: 0.4
                    color: defaultPalette.highlight

                    AnimationBehavior on width { NormalNumberAnimation { } }
                }
                Rectangle {
                    id: cachedEventsBar

                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin:
                        averageEvtHeight * chatView.largestVisibleIndex
                    implicitHeight: 2
                    width: chatView.largestVisibleIndex < 0 ? 0 :
                        averageEvtHeight
                        * (chatView.count - chatView.largestVisibleIndex)

                    color: defaultPalette.highlight

                    AnimationBehavior on width { FastNumberAnimation { } }
                }
            }
            handle: Rectangle {
                anchors.centerIn: parent
                color: defaultPalette.button
                border.color: defaultPalette.buttonText
                border.width: 1
                implicitWidth: 14
                implicitHeight: 8
                visible: chatView.count > 0
            }
        }

        maximumValue: 10.0
        minimumValue: -10.0

        activeFocusOnPress: false
        activeFocusOnTab: false
        // wheelEnabled: false // Only available in QQC 1.6, Qt 5.10

        onPressedChanged: {
            if (!pressed)
                value = 0
        }

        onValueChanged: {
            if (value)
                chatView.flick(0, parent.height * value)
        }
        Component.onCompleted: {
            // Continue scrolling while the shuttle is held out of 0
            chatView.flickEnded.connect(shuttleDial.valueChanged)
        }
    }

    MouseArea {
        id: scrollerArea
        anchors.top: chatScrollView.top
        anchors.bottom: chatScrollView.bottom
        anchors.right: parent.right
        width: settings.use_shuttle_dial ? shuttleDial.width
                                         : chatScrollView.width - chatView.width
        acceptedButtons: Qt.NoButton
        // FIXME: propagate wheel event to chatView
        onWheel: { wheel.accepted = settings.use_shuttle_dial }

        hoverEnabled: true
    }

    Rectangle {
        anchors.right: scrollerArea.left
        anchors.top: chatScrollView.top
        width: childrenRect.width + 3
        height: childrenRect.height + 3
        color: defaultPalette.window
        opacity: chatView.largestVisibleIndex >= 0
            && (scrollerArea.containsMouse || scrollAnimation.running)
            ? 0.9 : 0
        AnimationBehavior on opacity { FastNumberAnimation { } }

        Label {
            font.bold: true
            font.family: settings.font.family
            font.pointSize: settings.font.pointSize
            color: disabledPalette.text
            renderType: settings.render_type
            text: (chatView.largestVisibleIndex === 0
                   ? qsTr("Latest events") : qsTr("%Ln events back from now","",
                                                  chatView.largestVisibleIndex))
                  + '\n' + qsTr("%Ln cached", "", chatView.count)
            horizontalAlignment: Label.AlignRight
        }
    }

    Rectangle {
        id: scrollindicator
        opacity: chatView.atYEnd ? 0 : 0.5
        color: defaultPalette.text
        height: 30
        radius: height/2
        width: height
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: width/2
        anchors.bottomMargin: chatView.atYEnd ? -height : height/2
        AnimationBehavior on opacity {
            NormalNumberAnimation { easing.type: Easing.OutQuad }
        }
        AnimationBehavior on anchors.bottomMargin {
            NormalNumberAnimation { easing.type: Easing.OutQuad }
        }
        Image {
            anchors.fill: parent
            source: "qrc:///scrolldown.svg"
        }
        MouseArea {
            anchors.fill: parent
            onClicked: {
                chatView.positionViewAtBeginning()
                chatView.saveViewport()
            }
            cursorShape: Qt.PointingHandCursor
        }
    }
}
