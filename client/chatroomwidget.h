/**************************************************************************
 *                                                                        *
 * Copyright (C) 2015 Felix Rohrbach <kde@fxrh.de>                        *
 *                                                                        *
 * This program is free software; you can redistribute it and/or          *
 * modify it under the terms of the GNU General Public License            *
 * as published by the Free Software Foundation; either version 3         *
 * of the License, or (at your option) any later version.                 *
 *                                                                        *
 * This program is distributed in the hope that it will be useful,        *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 * GNU General Public License for more details.                           *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 *                                                                        *
 **************************************************************************/

#pragma once

#include "quaternionroom.h"
#include "chatedit.h"

#include <settings.h>

#include <QtWidgets/QWidget>
#include <QtWidgets/QToolButton>
#include <QtCore/QBasicTimer>
#include <QModelIndex>

#ifndef USE_QQUICKWIDGET
#define DISABLE_QQUICKWIDGET
#endif

class MessageEventModel;
class ImageProvider;

class QFrame;
#ifdef DISABLE_QQUICKWIDGET
class QQuickView;
#else
class QQuickWidget;
#endif
class QLabel;
class QAction;
class QTextDocument;
class QMimeData;
class QTemporaryFile;

class ChatRoomWidget: public QWidget
{
        Q_OBJECT
    public:
        using completions_t = ChatEdit::completions_t;

        explicit ChatRoomWidget(QWidget* parent = nullptr);

        void enableDebug();
        bool pendingMarkRead() const;

        completions_t findCompletionMatches(const QString& pattern) const;
        Q_INVOKABLE Qt::KeyboardModifiers getModifierKeys() const;

    signals:
        void resourceRequested(const QString& idOrUri,
                               const QString& action = {});
        void roomSettingsRequested();
        void showStatusMessage(const QString& message, int timeout = 0) const;
        void readMarkerMoved();
        void readMarkerCandidateMoved();
        void pageUpPressed();
        void pageDownPressed();
        void openExternally(int currentIndex);
        void showDetails(int currentIndex);
        void scrollViewTo(int currentIndex);
        void animateMessage(int currentIndex);
        void refer(int index);

    public slots:
        void setRoom(QuaternionRoom* room);
        void spotlightEvent(QString eventId);

        void insertMention(Quotient::User* user);
        void focusInput();

        void typingChanged();
        void onMessageShownChanged(const QString& eventId, bool shown);
        void markShownAsRead();
        void saveFileAs(QString eventId);
        void quote(const QString& htmlText);
        void reply(int currentIndex);
        void showMenu(int index, const QString& hoveredLink, const QString& selectedText, bool showingDetails);
        void reactionButtonClicked(const QString& eventId, const QString& key);
        void fileDrop(const QString& url);
        void htmlDrop(const QString& html);
        void textDrop(const QString& text);
        void setGlobalSelectionBuffer(QString text);

    private slots:
        void sendInput();
        void encryptionChanged();
        /// Set a line just above the message input, with optional list of
        /// member displaynames
        void setHudHtml(const QString& htmlCaption,
                        const QStringList& plainTextNames = {});

    private:
        enum InputModes {
            Simple,
            RichReply,
        };

        enum TextTypes {
            Default,
            Plain,
            Html,
            Markdown,
        };

        // Data
        MessageEventModel* m_messageModel;
        QuaternionRoom* m_currentRoom;
        ImageProvider* m_imageProvider;
        QTemporaryFile* m_fileToAttach;

        // Settings
        Quotient::SettingsGroup m_uiSettings;

        // Controls
#ifdef DISABLE_QQUICKWIDGET
        using timelineWidget_t = QQuickView;
#else
        using timelineWidget_t = QQuickWidget;
#endif
        timelineWidget_t* m_timelineWidget;
        QLabel* m_hudCaption; //< For typing and completion notifications
        QToolButton* m_referringInputIndicator;
        QAction* m_attachAction;
        ChatEdit* m_chatEdit;

        // Supplementary/cache data members
        using timeline_index_t = Quotient::TimelineItem::index_t;
        std::vector<timeline_index_t> indicesOnScreen;
        timeline_index_t indexToMaybeRead;
        QBasicTimer maybeReadTimer;
        bool readMarkerOnScreen;
        int inputMode;
        QModelIndex inputReference;
        QString attachedFileName;
        QString selectedText;

        void clearReferringInputMode();
        void setReferringInputMode(const int newInputMode, const int referredIndex, const QString& icon_name);

        void reStartShownTimer();
        void sendFile();
        void sendMessage(const QString& text, int textType = Default);
        [[nodiscard]] QString sendCommand(const QStringRef& command,
                                          const QString& argString);

        void timerEvent(QTimerEvent* qte) override;
        void resizeEvent(QResizeEvent*) override;
        void keyPressEvent(QKeyEvent*) override;

        int maximumChatEditHeight() const;
};
