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

#include "userlistmodel.h"

#include <QtCore/QDebug>
#include <QtCore/QVector>
#include <QtGui/QPixmap>

#include "lib/connection.h"
#include "lib/room.h"
#include "lib/user.h"

UserListModel::UserListModel(QObject* parent)
    : QAbstractListModel(parent)
    , m_connection(nullptr)
    , m_currentRoom(nullptr)
{ }

UserListModel::~UserListModel()
{
}

void UserListModel::setConnection(QMatrixClient::Connection* connection)
{
    m_connection = connection;
}

void UserListModel::setRoom(QMatrixClient::Room* room)
{
    beginResetModel();
    if( m_currentRoom )
    {
        disconnect( m_currentRoom, &QMatrixClient::Room::userAdded, this, &UserListModel::userAdded );
        disconnect( m_currentRoom, &QMatrixClient::Room::userRemoved, this, &UserListModel::userRemoved );
        for( QMatrixClient::User* user: m_users )
        {
            disconnect( user, &QMatrixClient::User::nameChanged, this, &UserListModel::userRenamed );
            disconnect( user, &QMatrixClient::User::avatarChanged, this, &UserListModel::avatarChanged );
        }
        m_users.clear();
    }
    m_currentRoom = room;
    if( m_currentRoom )
    {
        connect( m_currentRoom, &QMatrixClient::Room::userAdded, this, &UserListModel::userAdded );
        connect( m_currentRoom, &QMatrixClient::Room::userRemoved, this, &UserListModel::userRemoved );
        m_users = m_currentRoom->users().values();
        for( QMatrixClient::User* user: m_users )
        {
            connect( user, &QMatrixClient::User::nameChanged, this, &UserListModel::userRenamed );
            connect( user, &QMatrixClient::User::avatarChanged, this, &UserListModel::avatarChanged );
        }
        qDebug() << m_users.count();
    }
    endResetModel();
}

QVariant UserListModel::data(const QModelIndex& index, int role) const
{
    if( !index.isValid() )
        return QVariant();

    if( index.row() >= m_users.count() )
    {
        qDebug() << "Something's wrong: index.row() >= m_users.count()";
        return QVariant();
    }
    QMatrixClient::User* user = m_users.at(index.row());
    if( role == Qt::DisplayRole )
    {
        return m_currentRoom->roomMembername(user);
    }
    if( role == Qt::DecorationRole )
    {
        return user->avatar(25,25);
    }
    return QVariant();
}

int UserListModel::rowCount(const QModelIndex& parent) const
{
    if( parent.isValid() )
        return 0;

    return m_users.count();
}

void UserListModel::userAdded(QMatrixClient::User* user)
{
    beginInsertRows(QModelIndex(), m_users.count(), m_users.count());
    m_users.append(user);
    endInsertRows();
    connect( user, &QMatrixClient::User::avatarChanged, this, &UserListModel::avatarChanged );
}

void UserListModel::userRemoved(QMatrixClient::User* user)
{
    int pos = m_users.indexOf(user);
    beginRemoveRows(QModelIndex(), pos, pos);
    m_users.removeAt(pos);
    endRemoveRows();
    disconnect( user, &QMatrixClient::User::avatarChanged, this, &UserListModel::avatarChanged );
}

void UserListModel::userRenamed(QMatrixClient::User *user, QString /* unused */)
{
    int pos = m_users.indexOf(user);
    emit dataChanged(index(pos), index(pos), {Qt::DisplayRole} );
}

void UserListModel::avatarChanged(QMatrixClient::User* user)
{
    int pos = m_users.indexOf(user);
    emit dataChanged(index(pos), index(pos), {Qt::DecorationRole} );
}

