import QtQuick 2.0
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.1
import QtMultimedia 5.6

Attachment {
    property bool autoPlay

    onDownloadedChanged: {
        if (downloaded) {
            if (autoPlay)
                audioContent.play()
            timeSlider.enabled = true
        }
    }

    Audio {
        id: audioContent
        source: progressInfo.localPath
        autoLoad: false
        audioRole: Audio.MusicRole

        onPositionChanged: {
            pos.text = Math.floor(position / 60000) + ":" +
                ('0' + Math.floor(position / 1000 % 60)).slice(-2) + " / " +
                Math.floor(duration / 60000) + ":" +
                ('0' + Math.floor(duration / 1000 % 60)).slice(-2)
            if (!timeSlider.pressed)
                timeSlider.value = position / duration
        }

        onPlaying: playLabel.text = "▮▮"
        onPaused: playLabel.text = "▶ "
        onStopped: playLabel.text = "▶ "
    }

    RowLayout {
        id: controlls
        anchors.top: audioContent.bottom
        width: parent.width

        Label {
            id: playLabel
            text: "▶ "

            MouseArea {
                acceptedButtons: Qt.LeftButton
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor

                onClicked: {
                    if (!downloaded) {
                        autoPlay = true
                        room.downloadFile(eventId)
                        return
                    }

                    if (audioContent.playbackState === Audio.PlayingState)
                        audioContent.pause()
                    else
                        audioContent.play()
                }
            }
        }

        Label {
            id: pos
            text: "-:-- / -:--"
        }

        Slider {
            id: timeSlider
            wheelEnabled: false
            Layout.fillWidth: true
            enabled: false
            onValueChanged: audioContent.seek(value * audioContent.duration)
        }

        Slider {
            value: 1
            onValueChanged: audioContent.volume = value
        }
    }

    ProgressBar {
        visible: progressInfo && progressInfo.started
        width: parent.width

        value: progressInfo ? progressInfo.progress / progressInfo.total : -1
        indeterminate: !progressInfo || progressInfo.progress < 0
    }

    RowLayout {
        anchors.top: controlls.bottom
        width: parent.width
        spacing: 2

        Button {
            text: qsTr("Cancel")
            visible: progressInfo.started
            onClicked: room.cancelFileTransfer(eventId)
        }

        Button {
            text: qsTr("Open externally")
            onClicked: openExternally()
        }
        Button {
            text: qsTr("Save as...")
            onClicked: controller.saveFileAs(eventId)
        }
    }
}
