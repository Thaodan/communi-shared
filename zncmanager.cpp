/*
* Copyright (C) 2008-2013 The Communi Project
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include "zncmanager.h"
#include <ircsession.h>
#include <ircmessage.h>
#include <ircchannel.h>
#include <ircsender.h>

ZncManager::ZncManager(QObject* parent) : QObject(parent)
{
    d.model = 0;
    d.channel = 0;
    d.timestamp = 0;
    d.playback = false;
    d.timestamper.invalidate();
    d.timeStampFormat = "[hh:mm:ss]";
    setModel(qobject_cast<IrcChannelModel*>(parent));
}

ZncManager::~ZncManager()
{
}

bool ZncManager::isPlaybackActive() const
{
    return d.playback;
}

QString ZncManager::playbackTarget() const
{
    return d.target;
}

IrcChannelModel* ZncManager::model() const
{
    return d.model;
}

void ZncManager::setModel(IrcChannelModel* model)
{
    if (d.model != model) {
        if (d.model && d.model->session()) {
            IrcSession* session = d.model->session();
            disconnect(session, SIGNAL(connected()), this, SLOT(onConnected()));
            disconnect(session, SIGNAL(capabilities(QStringList,QStringList*)), this, SLOT(onCapabilities(QStringList,QStringList*)));
            session->removeMessageFilter(this);
        }
        d.model = model;
        if (d.model && d.model->session()) {
            IrcSession* session = d.model->session();
            connect(session, SIGNAL(connected()), this, SLOT(onConnected()));
            connect(session, SIGNAL(capabilities(QStringList,QStringList*)), this, SLOT(onCapabilities(QStringList,QStringList*)));
            session->installMessageFilter(this);
        }
        emit modelChanged(model);
    }
}

QString ZncManager::timeStampFormat() const
{
    return d.timeStampFormat;
}

void ZncManager::setTimeStampFormat(const QString& format)
{
    if (d.timeStampFormat != format) {
        d.timeStampFormat = format;
        emit timeStampFormatChanged(format);
    }
}

bool ZncManager::messageFilter(IrcMessage* message)
{
    if (d.timestamp > 0 && d.timestamper.isValid()) {
        long elapsed = d.timestamper.elapsed() / 1000;
        if (elapsed > 0) {
            d.timestamp += elapsed;
            d.timestamper.restart();
        }
    }

    if (message->type() == IrcMessage::Private) {
        IrcSender sender = message->sender();
        if (sender.name() == QLatin1String("***") && sender.user() == QLatin1String("znc")) {
            IrcPrivateMessage* privMsg = static_cast<IrcPrivateMessage*>(message);
            QString content = privMsg->message();
            if (content == QLatin1String("Buffer Playback...")) {
                if (!d.playback) {
                    d.playback = true;
                    emit playbackActiveChanged(d.playback);
                }
                if (d.target != privMsg->target()) {
                    d.target = privMsg->target();
                    d.channel = d.model->channel(d.target);
                    emit playbackTargetChanged(d.target);
                }
                return false;
            } else if (content == QLatin1String("Playback Complete.")) {
                if (d.playback) {
                    d.playback = false;
                    emit playbackActiveChanged(d.playback);
                }
                if (!d.target.isEmpty()) {
                    d.channel = 0;
                    d.target.clear();
                    emit playbackTargetChanged(d.target);
                }
                return false;
            }
        }
    } else if (message->type() == IrcMessage::Notice) {
        if (message->sender().name() == "*communi") {
            d.timestamp = static_cast<IrcNoticeMessage*>(message)->message().toLong();
            d.timestamper.restart();
            return true;
        }
    }

    if (d.playback && d.channel) {
        switch (message->type()) {
        case IrcMessage::Private:
            return processMessage(static_cast<IrcPrivateMessage*>(message));
        case IrcMessage::Notice:
            return processNotice(static_cast<IrcNoticeMessage*>(message));
        default:
            break;
        }
    }
    return false;
}

bool ZncManager::processMessage(IrcPrivateMessage* message)
{
    QString msg = message->message();
    int idx = msg.indexOf(" ");
    if (idx != -1) {
        QDateTime timeStamp = QDateTime::fromString(msg.left(idx), d.timeStampFormat);
        if (timeStamp.isValid()) {
            msg.remove(0, idx + 1);

            if (message->sender().name() == "*buffextras") {
                idx = msg.indexOf(" ");
                IrcSender sender(msg.left(idx));
                QString content = msg.mid(idx + 1);

                IrcMessage* tmp = 0;
                if (content.startsWith("joined")) {
                    tmp = IrcMessage::fromParameters(sender.prefix(), "JOIN", QStringList() << message->target(), message->session());
                } else if (content.startsWith("parted")) {
                    QString reason = content.mid(content.indexOf("[") + 1);
                    reason.chop(1);
                    tmp = IrcMessage::fromParameters(sender.prefix(), "PART", QStringList() << reason , message->session());
                } else if (content.startsWith("quit")) {
                    QString reason = content.mid(content.indexOf("[") + 1);
                    reason.chop(1);
                    tmp = IrcMessage::fromParameters(sender.prefix(), "QUIT", QStringList() << reason , message->session());
                } else if (content.startsWith("is")) {
                    QStringList tokens = content.split(" ", QString::SkipEmptyParts);
                    tmp = IrcMessage::fromParameters(sender.prefix(), "NICK", QStringList() << tokens.last() , message->session());
                } else if (content.startsWith("set")) {
                    QStringList tokens = content.split(" ", QString::SkipEmptyParts);
                    QString user = tokens.takeLast();
                    QString mode = tokens.takeLast();
                    tmp = IrcMessage::fromParameters(sender.prefix(), "MODE", QStringList() << message->target() << mode << user, message->session());
                } else if (content.startsWith("changed")) {
                    QString topic = content.mid(content.indexOf(":") + 2);
                    tmp = IrcMessage::fromParameters(sender.prefix(), "TOPIC", QStringList() << message->target() << topic, message->session());
                } else if (content.startsWith("kicked")) {
                    QString reason = content.mid(content.indexOf("[") + 1);
                    reason.chop(1);
                    QStringList tokens = content.split(" ", QString::SkipEmptyParts);
                    tmp = IrcMessage::fromParameters(sender.prefix(), "KICK", QStringList() << message->target() << tokens.value(2) << reason, message->session());
                }
                if (tmp) {
                    tmp->setTimeStamp(timeStamp);
                    QMetaObject::invokeMethod(d.channel, "messageReceived", Q_ARG(IrcMessage*, tmp));
                    tmp->deleteLater();
                    return true;
                }
            }

            if (message->isAction())
                msg = QString("\1ACTION %1\1").arg(msg);
            else if (message->isRequest())
                msg = QString("\1%1\1").arg(msg);
            message->setParameters(QStringList() << message->target() << msg);
            message->setTimeStamp(timeStamp);
        }
    }
    return false;
}

bool ZncManager::processNotice(IrcNoticeMessage* message)
{
    QString msg = message->message();
    int idx = msg.indexOf(" ");
    if (idx != -1) {
        QDateTime timeStamp = QDateTime::fromString(msg.left(idx), d.timeStampFormat);
        if (timeStamp.isValid()) {
            message->setTimeStamp(timeStamp);
            msg.remove(0, idx + 1);
            if (message->isReply())
                msg = QString("\1%1\1").arg(msg);
            message->setParameters(QStringList() << message->target() << msg);
        }
    }
    return false;
}

void ZncManager::onConnected()
{
    d.timestamper.invalidate();
}

void ZncManager::onCapabilities(const QStringList& available, QStringList* request)
{
    if (available.contains("communi")) {
        request->append("communi");
        request->append(QString("communi/%1").arg(d.timestamp));
    }
}
