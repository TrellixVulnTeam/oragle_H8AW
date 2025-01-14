/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef LabelsNodeList_h
#define LabelsNodeList_h

#include "LabelableElement.h"
#include "LiveNodeList.h"

namespace WebCore {

class LabelsNodeList final : public LiveNodeList {
public:
    static PassRef<LabelsNodeList> create(LabelableElement& forNode, Type type, const AtomicString&)
    {
        ASSERT_UNUSED(type, type == Type::LabelsNodeListType);
        return adoptRef(*new LabelsNodeList(forNode));
    }
    ~LabelsNodeList();

protected:
    explicit LabelsNodeList(LabelableElement& forNode);

    virtual bool nodeMatches(Element*) const override;
};

} // namespace WebCore

#endif // LabelsNodeList_h
