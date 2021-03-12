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

#include "chatroomwidget.h"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QAction>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtGui/QClipboard>
#include <QtGui/QTextCursor> // for last-minute message fixups before sending
#include <QtGui/QTextDocumentFragment> // to produce plain text from /html
#include <QtGui/QDesktopServices>

#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#ifdef DISABLE_QQUICKWIDGET
#include <QtQuick/QQuickView>
#else
#include <QtQuickWidgets/QQuickWidget>
#endif
#include <QtCore/QRegularExpression>
#include <QtCore/QStringBuilder>
#include <QtCore/QLocale>
#include <QtCore/QTemporaryFile>
#include <QtCore/QMimeData>

#include <events/roommessageevent.h>
#include <events/roompowerlevelsevent.h>
#include <events/reactionevent.h>
#include <csapi/message_pagination.h>
#include <user.h>
#include <connection.h>
#include <uri.h>
#include <settings.h>
#include "models/messageeventmodel.h"
#include "imageprovider.h"
#include "chatedit.h"
#include "htmlfilter.h"

static const auto DefaultPlaceholderText =
        ChatRoomWidget::tr("Choose a room to send messages or enter a command...");

static constexpr auto MaxNamesToShow = 5;
static constexpr auto SampleSizeForHud = 3;
Q_STATIC_ASSERT(MaxNamesToShow > SampleSizeForHud);

ChatRoomWidget::ChatRoomWidget(QWidget* parent)
    : QWidget(parent)
    , m_messageModel(new MessageEventModel(this))
    , m_currentRoom(nullptr)
    , m_uiSettings("UI")
    , indexToMaybeRead(-1)
    , readMarkerOnScreen(false)
{
    {
        using namespace Quotient;
        qmlRegisterUncreatableType<QuaternionRoom>("Quotient", 1, 0, "Room",
            "Room objects can only be created by libQuotient");
        qmlRegisterUncreatableType<User>("Quotient", 1, 0, "User",
            "User objects can only be created by libQuotient");
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
        qmlRegisterAnonymousType<GetRoomEventsJob>("Quotient", 1);
        qmlRegisterAnonymousType<MessageEventModel>("Quotient", 1);
#else
        qmlRegisterType<GetRoomEventsJob>();
        qmlRegisterType<MessageEventModel>();
#endif
        qRegisterMetaType<GetRoomEventsJob*>("GetRoomEventsJob*");
        qRegisterMetaType<User*>("User*");
        qmlRegisterType<Settings>("Quotient", 1, 0, "Settings");
        qmlRegisterUncreatableType<RoomMessageEvent>("Quotient", 1, 0,
            "RoomMessageEvent", "RoomMessageEvent is uncreatable");
    }

    m_timelineWidget = new timelineWidget_t;
    qDebug() << "Rendering QML with"
             << timelineWidget_t::staticMetaObject.className();
    auto* qmlContainer =
#ifdef DISABLE_QQUICKWIDGET
            QWidget::createWindowContainer(m_timelineWidget, this);
#else
            m_timelineWidget;
#endif // Use different objects but the same method with the same parameters
    qmlContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_timelineWidget->setResizeMode(timelineWidget_t::SizeRootObjectToView);

    m_imageProvider = new ImageProvider();
    m_timelineWidget->engine()
            ->addImageProvider(QStringLiteral("mtx"), m_imageProvider);

    auto* ctxt = m_timelineWidget->rootContext();
    ctxt->setContextProperty(QStringLiteral("messageModel"), m_messageModel);
    ctxt->setContextProperty(QStringLiteral("controller"), this);
    ctxt->setContextProperty(QStringLiteral("debug"), QVariant(false));
    ctxt->setContextProperty(QStringLiteral("room"), nullptr);

    m_timelineWidget->setSource(QUrl("qrc:///qml/Timeline.qml"));

    {
        m_hudCaption = new QLabel();
        m_hudCaption->setWordWrap(true);
        auto f = m_hudCaption->font();
        f.setItalic(true);
        m_hudCaption->setFont(f);
        m_hudCaption->setTextFormat(Qt::RichText);
    }

    m_referringInputIndicator = new QToolButton();
    m_referringInputIndicator->setAutoRaise(true);
    m_referringInputIndicator->setIconSize(QSize(16,16));
    m_referringInputIndicator->hide();
    connect(m_referringInputIndicator, &QToolButton::clicked, this, [this] {
        clearReferringInputMode();
    });

    auto attachButton = new QToolButton();
    attachButton->setAutoRaise(true);
    m_attachAction = new QAction(QIcon::fromTheme("mail-attachment"),
                                 tr("Attach"), attachButton);
    m_attachAction->setCheckable(true);
    m_attachAction->setDisabled(true);
    connect(m_attachAction, &QAction::triggered, this, [this] (bool checked) {
        if (checked)
        {
            attachedFileName =
                    QFileDialog::getOpenFileName(this, tr("Attach file"));
        } else {
            if (m_fileToAttach->isOpen())
                m_fileToAttach->remove();
            attachedFileName.clear();
        }

        if (!attachedFileName.isEmpty())
        {
            m_chatEdit->setPlaceholderText(
                tr("Add a message to the file or just push Enter"));
            emit showStatusMessage(tr("Attaching %1").arg(attachedFileName));
        } else {
            m_attachAction->setChecked(false);
            m_chatEdit->setPlaceholderText(DefaultPlaceholderText);
            emit showStatusMessage(tr("Attaching cancelled"), 3000);
        }
    });
    attachButton->setDefaultAction(m_attachAction);

    m_fileToAttach = new QTemporaryFile(this);

    m_chatEdit = new ChatEdit(this);
    m_chatEdit->setPlaceholderText(DefaultPlaceholderText);
    m_chatEdit->setAcceptRichText(true); // m_uiSettings.get("rich_text_editor", false);
    m_chatEdit->setMaximumHeight(maximumChatEditHeight());
    connect(m_chatEdit, &KChatEdit::returnPressed, this,
            &ChatRoomWidget::sendInput);
    connect(m_chatEdit, &KChatEdit::copyRequested, this, [=] {
        QApplication::clipboard()->setText(
            m_chatEdit->textCursor().hasSelection()
                ? m_chatEdit->textCursor().selectedText()
                : selectedText);
    });
    connect(m_chatEdit, &ChatEdit::insertFromMimeDataRequested,
            this, [=] (const QMimeData* source) {
        if (m_fileToAttach->isOpen() || m_currentRoom == nullptr)
            return;
        m_fileToAttach->open();

        qvariant_cast<QImage>(source->imageData()).save(m_fileToAttach, "PNG");

        attachedFileName = m_fileToAttach->fileName();
        m_attachAction->setChecked(true);
        m_chatEdit->setPlaceholderText(
            tr("Add a message to the file or just push Enter"));
        emit showStatusMessage(tr("Attaching an image from clipboard"));
    });
    connect(m_chatEdit, &ChatEdit::proposedCompletion, this,
        [this](QStringList matches, int pos) {
            // If the completion list is MaxNamesToShow or shorter, show all
            // of it; if it's longer, show SampleSizeForHud entries and
            // append how many more matches are there.

            if (matches.isEmpty()) {
                setHudHtml(tr("No completions"));
                return;
            }

            // Save the size before manipulations on matches
            const int extraSize = matches.size() - SampleSizeForHud;
            if (pos > 0 && pos >= matches.size() - SampleSizeForHud)
                matches.back() +=
                    tr("(end of list)",
                       "Marks the last entry in the completion list");

            // Replenish the tail of the list from the beginning, if needed
            std::rotate(matches.begin(), matches.begin() + pos,
                        matches.end());
            if (matches.size() > MaxNamesToShow) {
                const auto moreIt = matches.begin() + SampleSizeForHud;
                *moreIt = tr("%Ln more completions", "", extraSize);
                matches.erase(moreIt + 1, matches.end());
            }
            setHudHtml(tr("Next completion:"), matches);
        });
    // When completion is cancelled, show typing users, if any
    connect(m_chatEdit, &ChatEdit::cancelledCompletion,
            this, &ChatRoomWidget::typingChanged);

    {
        QString styleSheet;
        const auto& fontFamily =
            m_uiSettings.get<QString>("Fonts/timeline_family");
        if (!fontFamily.isEmpty())
            styleSheet += "font-family: " + fontFamily + ";";
        const auto& fontPointSize =
            m_uiSettings.value("Fonts/timeline_pointSize");
        if (fontPointSize.toReal() > 0.0)
            styleSheet += "font-size: " + fontPointSize.toString() + "pt;";
        if (!styleSheet.isEmpty())
            setStyleSheet(styleSheet);
    }

    auto* layout = new QVBoxLayout();
    layout->addWidget(qmlContainer);
    layout->addWidget(m_hudCaption);
    {
        auto inputLayout = new QHBoxLayout;
        inputLayout->addWidget(m_referringInputIndicator);
        inputLayout->addWidget(attachButton);
        inputLayout->addWidget(m_chatEdit);
        layout->addLayout(inputLayout);
    }
    setLayout(layout);
}

void ChatRoomWidget::enableDebug()
{
    QQmlContext* ctxt = m_timelineWidget->rootContext();
    ctxt->setContextProperty(QStringLiteral("debug"), true);
}

void ChatRoomWidget::setRoom(QuaternionRoom* room)
{
    if (m_currentRoom == room) {
        focusInput();
        return;
    }

    if( m_currentRoom )
    {
        m_currentRoom->setDisplayed(false);
        m_currentRoom->connection()->disconnect(this);
        m_currentRoom->disconnect( this );
    }
    readMarkerOnScreen = false;
    maybeReadTimer.stop();
    indicesOnScreen.clear();
    attachedFileName.clear();
    m_attachAction->setChecked(false);
    if (m_fileToAttach->isOpen())
        m_fileToAttach->remove();

    m_currentRoom = room;
    m_attachAction->setEnabled(m_currentRoom != nullptr);
    m_chatEdit->switchContext(room);
    if( m_currentRoom )
    {
        using namespace Quotient;
        m_imageProvider->setConnection(room->connection());
        focusInput();
        connect( m_currentRoom, &Room::typingChanged,
                 this, &ChatRoomWidget::typingChanged );
        connect( m_currentRoom, &Room::readMarkerMoved, this, [this] {
            const auto rm = m_currentRoom->readMarker();
            readMarkerOnScreen =
                rm != m_currentRoom->timelineEdge() &&
                std::lower_bound( indicesOnScreen.cbegin(), indicesOnScreen.cend(),
                                 rm->index() ) != indicesOnScreen.cend();
            reStartShownTimer();
            emit readMarkerMoved();
        });
        connect( m_currentRoom, &Room::encryption,
                 this, &ChatRoomWidget::encryptionChanged);
        connect(m_currentRoom->connection(), &Connection::loggedOut,
                this, [this]
        {
            qWarning() << "Logged out, escaping the room";
            setRoom(nullptr);
        });
        m_currentRoom->setDisplayed(true);
    } else
        m_imageProvider->setConnection(nullptr);
    m_timelineWidget->rootContext()
                    ->setContextProperty(QStringLiteral("room"), room);
    typingChanged();
    encryptionChanged();
    clearReferringInputMode();

    m_messageModel->changeRoom( m_currentRoom );
}

void ChatRoomWidget::spotlightEvent(QString eventId)
{
    auto index = m_messageModel->findRow(eventId);
    if (index >= 0) {
        emit scrollViewTo(index);
        emit animateMessage(index);
    } else
        setHudHtml("<font color=red>" % tr("Referenced message not found")
                   % "</font>");
}

void ChatRoomWidget::typingChanged()
{
    if (!m_currentRoom || m_currentRoom->usersTyping().isEmpty())
    {
        setHudHtml({});
        return;
    }
    const auto& usersTyping = m_currentRoom->usersTyping();
    QStringList typingNames;
    typingNames.reserve(MaxNamesToShow);
    const auto endIt = usersTyping.size() > MaxNamesToShow
                       ? usersTyping.cbegin() + SampleSizeForHud
                       : usersTyping.cend();
    for (auto it = usersTyping.cbegin(); it != endIt; ++it)
        typingNames << m_currentRoom->safeMemberName((*it)->id());

    if (usersTyping.size() > MaxNamesToShow) {
        typingNames.push_back(
            //: The number of users in the typing or completion list
            tr("%L1 more").arg(usersTyping.size() - SampleSizeForHud));
    }
    setHudHtml(tr("Currently typing:"), typingNames);
}

void ChatRoomWidget::encryptionChanged()
{
    m_chatEdit->setPlaceholderText(
        m_currentRoom
            ? m_currentRoom->usesEncryption()
                ? tr("Send a message (no end-to-end encryption support yet)...")
                : tr("Send a message (over %1) or enter a command...",
                     "%1 is the protocol used by the server (usually HTTPS)")
                  .arg(m_currentRoom->connection()->homeserver()
                       .scheme().toUpper())
            : DefaultPlaceholderText);
}

void ChatRoomWidget::setHudHtml(const QString& htmlCaption,
                                const QStringList& plainTextNames)
{
    if (htmlCaption.isEmpty()) { // Fast track
        m_hudCaption->clear();
        return;
    }

    auto hudText = htmlCaption;

    if (!plainTextNames.empty()) {
        QStringList namesToShow;
        namesToShow.reserve(plainTextNames.size());

        // Elide names that don't fit the HUD line width
        // NB: averageCharWidth() accounts for a list separator appended by
        // QLocale::createSeparatedList() appends. It would be ideal
        // to subtract the specific separator width but there's no way to get
        // the list separator from QLocale()
        // (https://bugreports.qt.io/browse/QTBUG-48510)
        const auto& fm = m_hudCaption->fontMetrics();
        for (const auto& name: plainTextNames) {
            auto elided =
                fm.elidedText(name, Qt::ElideMiddle,
                              m_hudCaption->width() - fm.averageCharWidth());
            // Make sure an elided name takes a new line
            namesToShow.push_back(
                (elided != name ? "<br/>" : "") + elided.toHtmlEscaped());
        }

        hudText += ' ' + namesToShow.join(", ");
    }
    m_hudCaption->setText(hudText);
}

void ChatRoomWidget::insertMention(Quotient::User* user)
{
    Q_ASSERT(m_currentRoom != nullptr);
    m_chatEdit->insertMention(
        user->displayname(m_currentRoom),
        Quotient::Uri(user->id()).toUrl(Quotient::Uri::MatrixToUri));
    m_chatEdit->setFocus();
}

void ChatRoomWidget::focusInput()
{
    m_chatEdit->setFocus();
}

/**
 * \brief Split the string into the specified number of parts
 * The function takes \p s and splits it into \p maxParts parts using \p sep
 * for the separator. Empty parts are skipped. If there are more than
 * \p maxParts parts in the string, the last returned part includes
 * the remainder of the string; if there are fewer parts, the missing parts
 * are filled with empty strings.
 * \return the vector of references to the original string, one reference for
 * each part.
 */
QVector<QString> lazySplitRef(const QString& s, QChar sep, int maxParts)
{
    QVector<QString> parts { maxParts };
    int pos = 0, nextPos = 0;
    for (; maxParts > 1 && (nextPos = s.indexOf(sep, pos)) > -1; --maxParts)
    {
        parts[parts.size() - maxParts] = s.mid(pos, nextPos - pos);
        while (s[++nextPos] == sep)
            ;
        pos = nextPos;
    }
    parts[parts.size() - maxParts] = s.mid(pos);
    return parts;
}

void ChatRoomWidget::sendFile()
{
    Q_ASSERT(m_currentRoom != nullptr);
    const auto& description = m_chatEdit->toPlainText();
    auto txnId = m_currentRoom->postFile(description.isEmpty()
                                             ? QUrl(attachedFileName).fileName()
                                             : description,
                                         QUrl::fromLocalFile(attachedFileName));

    if (m_fileToAttach->isOpen())
        m_fileToAttach->remove();
    attachedFileName.clear();
    m_attachAction->setChecked(false);
    m_chatEdit->setPlaceholderText(DefaultPlaceholderText);
}

void ChatRoomWidget::sendMessage(const QString& text, int textType)
{
    auto plainText = QTextDocumentFragment::fromHtml(text).toPlainText();
    bool sendHtml;
    QString htmlText;

    if (textType == Plain) {
        sendHtml = false;
        if (inputMode == RichReply) {
            htmlText = plainText;
            sendHtml = true;
        }
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    } else if (textType == Markdown || (textType == Default && m_uiSettings.get("auto_markdown", false))) {
        htmlText = HtmlFilter::qtToMatrix(text, m_currentRoom, HtmlFilter::ConvertMarkdown);
        sendHtml = true;
#endif
    } else {
        htmlText = HtmlFilter::qtToMatrix(text, m_currentRoom);
        sendHtml = true;
    }

    Q_ASSERT(!plainText.isEmpty());
    Q_ASSERT(!sendHtml || !htmlText.isEmpty());

    if (inputMode == RichReply) {
        using namespace Quotient;
        // generate quote in html
        const auto referredEventId = inputReference.data(MessageEventModel::EventIdRole).toString();
        const auto authorUser = inputReference.data(MessageEventModel::AuthorRole).value<User*>();
        const auto citation = inputReference.data().toString().remove(QRegExp("<mx-reply>.*</mx-reply>"));
        const auto htmlQuote = QStringLiteral(
            "<mx-reply><blockquote><a href=\"%1\">In reply to</a> <a href=\"%2\">%3</a><br />%4</blockquote></mx-reply>"
        ).arg(
            Uri(m_currentRoom->id().toUtf8(), referredEventId.toUtf8()).toUrl(Uri::MatrixToUri).toString(),
            Uri(authorUser->id()).toUrl(Uri::MatrixToUri).toString(),
            authorUser->displayname(m_currentRoom),
            citation
        );
        // derive plain text fallback
        const auto plainTextQuote = QLocale().quoteString(QTextDocumentFragment::fromHtml(citation).toPlainText()) + "\n";

        m_currentRoom->postEvent(
            new RoomMessageEvent(plainTextQuote + plainText, MessageEventType::Text,
                new EventContent::TextContent(
                    htmlQuote + htmlText, QStringLiteral("text/html"), EventContent::replyTo(referredEventId)
                )
            )
        );
        return;
    }

    if (sendHtml)
        m_currentRoom->postHtmlText(plainText, htmlText);
    else
        m_currentRoom->postPlainText(plainText);
}

static const auto NothingToSendMsg =
    ChatRoomWidget::tr("There's nothing to send");

QString ChatRoomWidget::sendCommand(const QStringRef& command,
                                    const QString& argString)
{
    static const auto ReFlags = QRegularExpression::DotMatchesEverythingOption
                                | QRegularExpression::DontCaptureOption;

    // FIXME: copy-paste from lib/util.cpp
    static const auto ServerPartPattern =
        QStringLiteral("(\\[[^]]+\\]|[-[:alnum:].]+)" // Either IPv6 address or
                                                      // hostname/IPv4 address
                       "(:\\d{1,5})?" // Optional port
        );
    static const auto UserIdPattern =
        QString("@[-[:alnum:]._=/]+:" % ServerPartPattern);

    static const QRegularExpression
        RoomIdRE { "^([#!][^:[:space:]]+):" % ServerPartPattern % '$', ReFlags },
        UserIdRE { '^' % UserIdPattern % '$', ReFlags };
    Q_ASSERT(RoomIdRE.isValid() && UserIdRE.isValid());

    // Commands available without a current room
    if (command == "join")
    {
        if (!argString.contains(RoomIdRE))
            return tr("/join argument doesn't look like a room ID or alias");
        emit resourceRequested(argString, "join");
        return {};
    }
    if (command == "quit")
    {
        qApp->closeAllWindows();
        return {};
    }
    // --- Add more roomless commands here
    if (!m_currentRoom)
    {
        return tr("There's no such /command outside of room.");
    }

    // Commands available only in the room context
    using namespace Quotient;
    if (command == "leave" || command == "part")
    {
        if (!argString.isEmpty())
            return tr("Sending a farewell message is not supported yet."
                      " If you intended to leave another room, switch to it"
                      " and type /leave there.");

        m_currentRoom->leaveRoom();
        return {};
    }
    if (command == "forget")
    {
        if (argString.isEmpty())
            return tr("/forget must be followed by the room id/alias,"
                      " even for the current room");
        if (!argString.contains(RoomIdRE))
            return tr("%1 doesn't look like a room id or alias").arg(argString);

        // Forget the specified room using the current room's connection
        m_currentRoom->connection()->forgetRoom(argString);
        return {};
    }
    if (command == "invite")
    {
        if (argString.isEmpty())
            return tr("/invite <memberId>");
        if (!argString.contains(UserIdRE))
            return tr("%1 doesn't look like a user ID").arg(argString);

        m_currentRoom->inviteToRoom(argString);
        return {};
    }
    if (command == "kick" || command == "ban")
    {
        const auto args = lazySplitRef(argString, ' ', 2);
        if (args.front().isEmpty())
            return tr("/%1 <userId> <reason>").arg(command.toString());
        if (!UserIdRE.match(args.front()).hasMatch())
            return tr("%1 doesn't look like a user id")
                    .arg(args.front());

        if (command == "ban")
            m_currentRoom->ban(args.front(), args.back());
        else {
            auto* const user = m_currentRoom->user(args.front());
            if (m_currentRoom->memberJoinState(user) != JoinState::Join)
                return tr("%1 is not a member of this room")
                    .arg(user ? user->fullName(m_currentRoom) : args.front());
            m_currentRoom->kickMember(user->id(), args.back());
        }
        return {};
    }
    if (command == "unban")
    {
        if (argString.isEmpty())
            return tr("/unban <userId>");
        if (!argString.contains(UserIdRE))
            return tr("/unban argument doesn't look like a user ID");

        m_currentRoom->unban(argString);
        return {};
    }
    if (command == "ignore" || command == "unignore")
    {
        if (argString.isEmpty())
            return tr("/ignore <userId>");
        if (!argString.contains(UserIdRE))
            return tr("/ignore argument doesn't look like a user ID");

        if (auto* user = m_currentRoom->user(argString))
        {
            if (command == "ignore")
                user->ignore();
            else
                user->unmarkIgnore();
            return {};
        }
        return tr("Couldn't find user %1 on the server").arg(argString);
    }
    using MsgType = RoomMessageEvent::MsgType;
    if (command == "me")
    {
        if (argString.isEmpty())
            return tr("/me needs an argument");
        m_currentRoom->postMessage(argString, MsgType::Emote);
        return {};
    }
    if (command == "notice")
    {
        if (argString.isEmpty())
            return tr("/notice needs an argument");
        m_currentRoom->postMessage(argString, MsgType::Notice);
        return {};
    }
    if (command == "shrug") // Peeked at Discord
    {
        m_currentRoom->postPlainText((argString.isEmpty() ? "" : argString + " ") +
                                     "¯\\_(ツ)_/¯");
        return {};
    }
    if (command == "roomname")
    {
        m_currentRoom->setName(argString);
        return {};
    }
    if (command == "topic")
    {
        m_currentRoom->setTopic(argString);
        return {};
    }
    if (command == "nick" || command == "mynick")
    {
        m_currentRoom->localUser()->rename(argString);
        return {};
    }
    if (command == "roomnick" || command == "myroomnick")
    {
        m_currentRoom->localUser()->rename(argString, m_currentRoom);
        return {};
    }
    if (command == "pm" || command == "msg")
    {
        const auto args = lazySplitRef(argString, ' ', 2);
        if (args.front().isEmpty() || (args.back().isEmpty() && command == "msg"))
            return tr("/%1 <memberId> <message>").arg(command.toString());
        if (RoomIdRE.match(args.front()).hasMatch() && command == "msg")
        {
            if (auto* room = m_currentRoom->connection()->room(args.front()))
            {
                room->postPlainText(args.back());
                return {};
            }
            return tr("%1 doesn't seem to have joined room %2")
                    .arg(m_currentRoom->localUser()->id(), args.front());
        }
        if (UserIdRE.match(args.front()).hasMatch())
        {
            if (args.back().isEmpty())
                m_currentRoom->connection()->requestDirectChat(args.front());
            else
                m_currentRoom->connection()->doInDirectChat(args.front(),
                    [msg=args.back()] (Room* dc) { dc->postPlainText(msg); });
            return {};
        }

        return tr("%1 doesn't look like a user id or room alias")
                .arg(args.front());
    }
    if (command == "plain") {
        static const auto CmdLen = QStringLiteral("/plain ").size();
        const auto& plainMsg = m_chatEdit->toPlainText().mid(CmdLen);
        if (plainMsg.isEmpty())
            return NothingToSendMsg;
        sendMessage(plainMsg, Plain);
        return {};
    }
    if (command == "html")
    {
        // Assuming Matrix HTML, convert it to Qt and load to a fragment in
        // order to produce a plain text version (maybe introduce
        // filterMatrixHtmlToPlainText() one day instead...); then convert
        // back to Matrix HTML to produce the (clean) rich text version
        // of the message
        const auto& [cleanQtHtml, errorPos, errorString] =
            HtmlFilter::matrixToQt(argString, m_currentRoom, true);
        if (errorPos != -1)
            return tr("At pos %1: %2",
                      "%1 is a position of the error; %2 is the error message")
                   .arg(errorPos).arg(errorString);

        sendMessage(cleanQtHtml, Html);
        return {};
    }
    if (command == "md") {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        // Select everything after /md and one whitespace character after it
        // (leading whitespaces have meaning in Markdown)
        QTextCursor c(m_chatEdit->document());
        c.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 4);
        c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        sendMessage(c.selection().toHtml(), Markdown);
        return {};
#else
        return tr("Your build of Quaternion doesn't support Markdown");
#endif
    }
    if (command == "query" || command == "dc")
    {
        if (argString.isEmpty())
            return tr("/%1 <memberId>").arg(command.toString());
        if (!argString.contains(UserIdRE))
            return tr("%1 doesn't look like a user id").arg(argString);

        m_currentRoom->connection()->requestDirectChat(argString);
        return {};
    }
    // --- Add more room commands here
    qDebug() << "Unknown command:" << command;
    return tr("Unknown /command. Use // to send this line literally");
}

void ChatRoomWidget::sendInput()
{
    if (!attachedFileName.isEmpty())
        sendFile();
    else {
        const auto& text = m_chatEdit->toPlainText();
        QString error;
        if (text.isEmpty())
            error = NothingToSendMsg;
        else if (text.startsWith('/') && !text.midRef(1).startsWith('/')) {
            QRegularExpression cmdSplit(
                    "(\\w+)(?:\\s+(.*))?",
                    QRegularExpression::DotMatchesEverythingOption);
            const auto& blanksMatch = cmdSplit.match(text, 1);
            error = sendCommand(blanksMatch.capturedRef(1),
                                blanksMatch.captured(2));
        } else if (!m_currentRoom)
            error = tr("You should select a room to send messages.");
        else {
            if (m_chatEdit->toPlainText().startsWith("//"))
                QTextCursor(m_chatEdit->document()).deleteChar();
            sendMessage(m_chatEdit->toHtml());
        }
        if (!error.isEmpty()) {
            emit showStatusMessage(error, 5000);
            return;
        }
    }

    m_chatEdit->saveInput();
    clearReferringInputMode();
}

ChatRoomWidget::completions_t
ChatRoomWidget::findCompletionMatches(const QString& pattern) const
{
    completions_t matches;
    if (m_currentRoom)
    {
        for(auto user: m_currentRoom->users() )
        {
            using Quotient::Uri;
            if (user->displayname(m_currentRoom)
                    .startsWith(pattern, Qt::CaseInsensitive)
                || user->id().startsWith(pattern, Qt::CaseInsensitive))
                matches.push_back({ user->displayname(m_currentRoom),
                                    Uri(user->id()).toUrl(Uri::MatrixToUri) });
        }
        std::sort(matches.begin(), matches.end(),
            [] (const auto& p1, const auto& p2)
                { return p1.first.localeAwareCompare(p2.first) < 0; });
    }
    return matches;
}

void ChatRoomWidget::saveFileAs(QString eventId)
{
    if (!m_currentRoom)
    {
        qWarning()
            << "ChatRoomWidget::saveFileAs without an active room ignored";
        return;
    }
    auto fileName = QFileDialog::getSaveFileName(
                this, tr("Save file as"),
                m_currentRoom->fileNameToDownload(eventId));
    if (!fileName.isEmpty())
        m_currentRoom->downloadFile(eventId, QUrl::fromLocalFile(fileName));
}

void ChatRoomWidget::onMessageShownChanged(const QString& eventId, bool shown)
{
    if (!m_currentRoom || !m_currentRoom->displayed())
        return;

    // A message can be auto-marked as read (as soon as the user is active), if:
    // 0. The read marker exists and is on the screen
    // 1. The message is shown on the screen now
    // 2. It's been the bottommost message on the screen for the last 1 second
    // 3. It's below the read marker

    const auto readMarker = m_currentRoom->readMarker();
    if (readMarker != m_currentRoom->timelineEdge() &&
            readMarker->event()->id() == eventId)
    {
        readMarkerOnScreen = shown;
        if (shown)
        {
            qDebug() << "Read marker is on-screen, at" << *readMarker;
            indexToMaybeRead = readMarker->index();
            reStartShownTimer();
        } else
        {
            qDebug() << "Read marker is off-screen";
            qDebug() << "Bottommost shown message index was" << indexToMaybeRead;
            maybeReadTimer.stop();
        }
    }

    const auto iter = m_currentRoom->findInTimeline(eventId);
    if (iter == m_currentRoom->timelineEdge())
    {
        qWarning() << "Event" << eventId
                   << "is not in the timeline (local echo?)";
        return;
    }
    const auto timelineIndex = iter->index();
    auto pos = std::lower_bound(indicesOnScreen.begin(), indicesOnScreen.end(),
                                timelineIndex);
    if (shown)
    {
        if (pos == indicesOnScreen.end() || *pos != timelineIndex)
        {
            indicesOnScreen.insert(pos, timelineIndex);
            if (timelineIndex == indicesOnScreen.back())
                reStartShownTimer();
        }
    } else
    {
        if (pos != indicesOnScreen.end() && *pos == timelineIndex)
            if (indicesOnScreen.erase(pos) == indicesOnScreen.end())
                reStartShownTimer();
    }
}

void ChatRoomWidget::quote(const QString& htmlText)
{
    const auto type = m_uiSettings.get<int>("quote_type");
    const auto defaultStyle = QStringLiteral("> \\1\n");
    const auto defaultRegex = QStringLiteral("(.+)(?:\n|$)");
    auto style = m_uiSettings.get<QString>("quote_style");
    auto regex = m_uiSettings.get<QString>("quote_regex");

    if (style.isEmpty())
        style = defaultStyle;
    if (regex.isEmpty())
        regex = defaultRegex;

    QTextDocument document;
    document.setHtml(htmlText);
    QString sendString;

    switch (type)
    {
        case 0:
            sendString = document.toPlainText()
                .replace(QRegularExpression(defaultRegex), defaultStyle);
            break;
        case 1:
            sendString = document.toPlainText()
                .replace(QRegularExpression(regex), style);
            break;
        case 2:
            sendString = QLocale().quoteString(document.toPlainText()) + "\n";
            break;
    }

    m_chatEdit->insertPlainText(sendString);
}

void ChatRoomWidget::clearReferringInputMode()
{
    inputMode = Simple;
    emit refer(-1);
    inputReference = QModelIndex();

    m_referringInputIndicator->hide();
}

void ChatRoomWidget::setReferringInputMode(const int newInputMode, const int referredIndex, const QString& icon_name)
{
    Q_ASSERT( newInputMode == RichReply );
    inputMode = newInputMode;
    inputReference = m_messageModel->index(referredIndex, 0);
    emit refer(referredIndex);

    m_referringInputIndicator->setIcon(QIcon::fromTheme(icon_name));
    m_referringInputIndicator->show();
}

void ChatRoomWidget::reply(int currentIndex)
{
    setReferringInputMode(RichReply, currentIndex, "mail-reply-sender");
    emit showStatusMessage(tr("Reply message"));
}

void ChatRoomWidget::showMenu(int index, const QString& hoveredLink,
                              const QString& selectedText, bool showingDetails)
{
    const auto modelIndex = m_messageModel->index(index, 0);
    const auto eventId = modelIndex.data(MessageEventModel::EventIdRole).toString();

    QMenu menu;

    const auto* plEvt =
        m_currentRoom->getCurrentState<Quotient::RoomPowerLevelsEvent>();
    const auto localUserId = m_currentRoom->localUser()->id();
    const int userPl = plEvt->powerLevelForUser(localUserId);
    const auto* modelUser =
        modelIndex.data(MessageEventModel::AuthorRole).value<Quotient::User*>();
    menu.addAction(QIcon::fromTheme("mail-reply-sender"), tr("Reply"), [=] {
        emit reply(index);
    });
    if (!plEvt || userPl >= plEvt->redact() || localUserId == modelUser->id()) {
        menu.addAction(QIcon::fromTheme("edit-delete"), tr("Redact"), [=] {
            m_currentRoom->redactEvent(eventId);
        });
    }
    if (!selectedText.isEmpty())
    {
        menu.addAction(tr("Copy selected text to clipboard"), [=] {
            QApplication::clipboard()->setText(selectedText);
        });
    }
    if (!hoveredLink.isEmpty())
    {
        menu.addAction(tr("Copy link to clipboard"), [=] {
            QApplication::clipboard()->setText(hoveredLink);
        });
    }
    menu.addAction(QIcon::fromTheme("link"), tr("Copy permalink to clipboard"), [=] {
        QApplication::clipboard()->setText("https://matrix.to/#/" +
            m_currentRoom->id() + "/" + QUrl::toPercentEncoding(eventId));
    });
    menu.addAction(QIcon::fromTheme("format-text-blockquote"),
                   tr("Quote", "a verb (do quote), not a noun (a quote)"), [=] {
        emit quote(modelIndex.data().toString());
    });
    auto a = menu.addAction(QIcon::fromTheme("view-list-details"), tr("Show details"), [=] {
        emit showDetails(index);
    });
    a->setCheckable(true);
    a->setChecked(showingDetails);

    const auto eventType = modelIndex.data(MessageEventModel::EventTypeRole).toString();
    if (eventType == "image" || eventType == "file")
    {
        const auto progressInfo = modelIndex.data(MessageEventModel::LongOperationRole)
            .value<Quotient::FileTransferInfo>();
        const bool downloaded = !progressInfo.isUpload && progressInfo.completed();

        menu.addSeparator();
        menu.addAction(QIcon::fromTheme("document-open"), tr("Open externally"), [=] {
            emit openExternally(index);
        });
        if (downloaded) {
            menu.addAction(QIcon::fromTheme("folder-open"), tr("Open Folder"), [=] {
                QDesktopServices::openUrl(progressInfo.localDir);
            });
            if (eventType == "image")
            {
                menu.addAction(tr("Copy image to clipboard"), [=] {
                    QApplication::clipboard()->setImage(QImage(progressInfo.localPath.path()));
                });
            }
        }
        else
        {
            menu.addAction(QIcon::fromTheme("edit-download"), tr("Download"), [=] {
                m_currentRoom->downloadFile(eventId);
            });
        }
        menu.addAction(QIcon::fromTheme("document-save-as"), tr("Save file as..."), [=] {
            saveFileAs(eventId);
        });
    }
    menu.exec(QCursor::pos());
}

void ChatRoomWidget::reactionButtonClicked(const QString& eventId, const QString& key)
{
    using namespace Quotient;
    const auto& annotations =
        m_currentRoom->relatedEvents(eventId, EventRelation::Annotation());

    for (const auto& a : annotations) {
        auto* e = eventCast<const ReactionEvent>(a);
        if (e != nullptr && e->relation().key == key
                && a->senderId() == m_currentRoom->localUser()->id()) {
            m_currentRoom->redactEvent(a->id());
            return;
        }
    }

    m_currentRoom->postReaction(eventId, key);
}

void ChatRoomWidget::setGlobalSelectionBuffer(QString text)
{
    if (QApplication::clipboard()->supportsSelection())
        QApplication::clipboard()->setText(text, QClipboard::Selection);

    selectedText = text;
}

void ChatRoomWidget::reStartShownTimer()
{
    if (!readMarkerOnScreen || indicesOnScreen.empty() ||
            indexToMaybeRead >= indicesOnScreen.back())
        return;

    maybeReadTimer.start(m_uiSettings.get<int>("maybe_read_timer", 1000), this);
    qDebug() << "Scheduled maybe-read message update:"
             << indexToMaybeRead << "->" << indicesOnScreen.back();
}

void ChatRoomWidget::timerEvent(QTimerEvent* qte)
{
    if (qte->timerId() != maybeReadTimer.timerId())
    {
        QWidget::timerEvent(qte);
        return;
    }
    maybeReadTimer.stop();
    // Only update the maybe-read message if we're tracking it
    if (readMarkerOnScreen && !indicesOnScreen.empty()
            && indexToMaybeRead < indicesOnScreen.back())
    {
        qDebug() << "Maybe-read message update:" << indexToMaybeRead
                 << "->" << indicesOnScreen.back();
        indexToMaybeRead = indicesOnScreen.back();
        emit readMarkerCandidateMoved();
    }
}

void ChatRoomWidget::resizeEvent(QResizeEvent*)
{
    m_chatEdit->setMaximumHeight(maximumChatEditHeight());
}

void ChatRoomWidget::keyPressEvent(QKeyEvent* event)
{
    // This only handles keypresses not handled by ChatEdit; in particular,
    // this means that PageUp/PageDown below are actually Ctrl-PageUp/PageDown
    switch(event->key()) {
        case Qt::Key_PageUp:
            emit pageUpPressed();
            break;
        case Qt::Key_PageDown:
            emit pageDownPressed();
            break;
    }
}

int ChatRoomWidget::maximumChatEditHeight() const
{
    return maximumHeight() / 3;
}

void ChatRoomWidget::markShownAsRead()
{
    // FIXME: a case when a single message doesn't fit on the screen.
    if (m_currentRoom && readMarkerOnScreen)
    {
        const auto iter = m_currentRoom->findInTimeline(indicesOnScreen.back());
        Q_ASSERT( iter != m_currentRoom->timelineEdge() );
        m_currentRoom->markMessagesAsRead((*iter)->id());
    }
}

bool ChatRoomWidget::pendingMarkRead() const
{
    if (!readMarkerOnScreen || !m_currentRoom)
        return false;

    const auto rm = m_currentRoom->readMarker();
    return rm != m_currentRoom->timelineEdge() && rm->index() < indexToMaybeRead;
}

void ChatRoomWidget::fileDrop(const QString& url)
{
    attachedFileName = QUrl(url).path();
    m_attachAction->setChecked(true);
    m_chatEdit->setPlaceholderText(
        tr("Add a message to the file or just push Enter"));
    emit showStatusMessage(tr("Attaching %1").arg(attachedFileName));
}

void ChatRoomWidget::htmlDrop(const QString &html)
{
    m_chatEdit->insertHtml(html);
}

void ChatRoomWidget::textDrop(const QString& text)
{
    m_chatEdit->insertPlainText(text);
}

Qt::KeyboardModifiers ChatRoomWidget::getModifierKeys() const
{
    return QGuiApplication::keyboardModifiers();
}
