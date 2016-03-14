/******************************************************************************
 * Copyright (C) 2015 Felix Rohrbach <kde@fxrh.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef QMATRIXCLIENT_ROOM_H
#define QMATRIXCLIENT_ROOM_H

#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QJsonObject>

#include "jobs/syncjob.h"
#include "joinstate.h"

namespace QMatrixClient
{
    class Event;
    class State;
    class Connection;
    class User;

    class Room: public QObject
    {
            Q_OBJECT
        public:
            /** Map of user names (potentially duplicate) to users */
            typedef QMultiHash<QString, User *> members_list_t;

            Room(Connection* connection, QString id);
            virtual ~Room();

            QString id() const;
            QList<Event*> messages() const;
            QString name() const;
            QStringList aliases() const;
            QString canonicalAlias() const;
            QString displayName() const;
            QString topic() const;
            JoinState joinState() const;
            QList<User*> usersTyping() const;
            QList<User*> usersLeft() const;

            members_list_t users() const;

            /**
             * @brief Produces a disambiguated name for a given user in
             * the context of the room.
             */
            QString roomMembername(User* u) const;

            void addMessage( Event* event );
            void addInitialState( State* state );
            void updateData( const SyncRoomData& data );
            void setJoinState( JoinState state );
            void getPreviousContent();

        public slots:
            void gotMessages(KJob* job);
            void userRenamed(User* user, QString oldName);

        signals:
            void newMessage(Event* event);
            void namesChanged(Room* room);
            void topicChanged();
            void userAdded(User* user);
            void userRemoved(User* user);
            void joinStateChanged(JoinState oldState, JoinState newState);
            void typingChanged();

        private:
            class Private;
            Private* d;
    };
}

#endif // QMATRIXCLIENT_ROOM_H
