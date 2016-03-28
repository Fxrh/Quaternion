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

#include "mainwindow.h"

#include <QtCore/QTimer>
#include <QtCore/QDebug>

#include "roomlistdock.h"
#include "userlistdock.h"
#include "chatroomwidget.h"
#include "logindialog.h"
#include "lib/jobs/geteventsjob.h"

MainWindow::MainWindow()
{
    connection = 0;
    roomListDock = new RoomListDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, roomListDock);
    userListDock = new UserListDock(this);
    addDockWidget(Qt::RightDockWidgetArea, userListDock);
    chatRoomWidget = new ChatRoomWidget(this);
    setCentralWidget(chatRoomWidget);
    connect( roomListDock, &RoomListDock::roomSelected, chatRoomWidget, &ChatRoomWidget::setRoom );
    connect( roomListDock, &RoomListDock::roomSelected, userListDock, &UserListDock::setRoom );
    show();
    QTimer::singleShot(0, this, SLOT(initialize()));
}

MainWindow::~MainWindow()
{
}

void MainWindow::initialize()
{
    LoginDialog dialog(this);
    if( dialog.exec() )
    {
        connection = dialog.connection();
        chatRoomWidget->setConnection(connection);
        userListDock->setConnection(connection);
        roomListDock->setConnection(connection);
        connect( connection, &QMatrixClient::Connection::connectionError, this, &MainWindow::connectionError );
        connect( connection, &QMatrixClient::Connection::syncDone, this, &MainWindow::gotEvents );
        connect( connection, &QMatrixClient::Connection::reconnected, this, &MainWindow::getNewEvents );
        connection->sync();
    }
}

void MainWindow::getNewEvents()
{
    //qDebug() << "getNewEvents";
    connection->sync(30*1000);
}

void MainWindow::gotEvents()
{
    // qDebug() << "newEvents";
    getNewEvents();
}

void MainWindow::connectionError(QString error)
{
    qDebug() << error;
    qDebug() << "reconnecting...";
    connection->reconnect();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (connection)
    {
        disconnect( connection, &QMatrixClient::Connection::syncDone, this, &MainWindow::gotEvents );
        disconnect( connection, &QMatrixClient::Connection::reconnected, this, &MainWindow::getNewEvents );
    }
    event->accept();
}


