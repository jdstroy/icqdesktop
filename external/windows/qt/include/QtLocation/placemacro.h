/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef PLACE_MACRO_H
#define PLACE_MACRO_H

#include <QtCore/QtGlobal>

QT_BEGIN_NAMESPACE

#define Q_DECLARE_D_FUNC(Class) \
    inline Class##Private *d_func(); \
    inline const Class##Private *d_func() const;\
    friend class Class##Private;

#define Q_DECLARE_COPY_CTOR(Class, BaseClass) \
    Class(const BaseClass &other);

#define Q_IMPLEMENT_D_FUNC(Class) \
    Class##Private *Class::d_func() { return reinterpret_cast<Class##Private *>(d_ptr.data()); } \
    const Class##Private *Class::d_func() const { return reinterpret_cast<const Class##Private *>(d_ptr.constData()); }

#define Q_IMPLEMENT_COPY_CTOR(Class, BaseClass) \
    Class::Class(const BaseClass &other) : BaseClass() { Class##Private::copyIfPossible(d_ptr, other); }

#define Q_DEFINE_PRIVATE_HELPER(Class, BaseClass, ClassType) \
    BaseClass##Private *clone() const { return new Class##Private(*this); } \
    static void copyIfPossible(QSharedDataPointer<BaseClass##Private> &d_ptr, const BaseClass &other) \
    { \
        if (other.type() == ClassType) \
            d_ptr = extract_d(other); \
        else \
            d_ptr = new Class##Private; \
    }

QT_END_NAMESPACE

#endif
