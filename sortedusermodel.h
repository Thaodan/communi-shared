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

#ifndef SORTEDUSERMODEL_H
#define SORTEDUSERMODEL_H

#include <QStringList>
#include <QSortFilterProxyModel>

class IrcChannel;

class SortedUserModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(IrcChannel* channel READ channel WRITE setChannel)

public:
    explicit SortedUserModel(QObject* parent = 0);

    IrcChannel* channel() const;
    void setChannel(IrcChannel* channel);

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const;

private:
    QStringList m_prefixes;
};

#endif // SORTEDUSERMODEL_H
