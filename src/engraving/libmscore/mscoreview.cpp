/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "mscoreview.h"
#include "score.h"
#include "page.h"
#include "text.h"

// Debug macro for element detection analysis
#define MSCOREVIEW_DEBUG 0

#if MSCOREVIEW_DEBUG
#include <cstdio>
#endif

using namespace mu;

namespace mu::engraving {
//---------------------------------------------------------
//   elementLower
//---------------------------------------------------------

static bool elementLower(const EngravingItem* e1, const EngravingItem* e2)
{
    if (!e1->selectable()) {
        return false;
    }
    if (!e2->selectable()) {
        return true;
    }
    return e1->z() < e2->z();
}

//---------------------------------------------------------
//   elementAt
//---------------------------------------------------------

EngravingItem* MuseScoreView::elementAt(const mu::PointF& p) const
{
    std::vector<EngravingItem*> el = elementsAt(p);
    EngravingItem* e = el.front();
    if (e && e->isPage()) {
        e = *std::next(el.begin());
    }
    return e;
}

//---------------------------------------------------------
//   point2page
//---------------------------------------------------------

Page* MuseScoreView::point2page(const mu::PointF& p) const
{
#if MSCOREVIEW_DEBUG
    printf("[MSCOREVIEW] point2page: checking point(%.1f, %.1f)\n", p.x(), p.y());
#endif
    
    if (score()->linearMode()) {
#if MSCOREVIEW_DEBUG
        printf("[MSCOREVIEW] point2page: linearMode, returning first page\n");
#endif
        return score()->pages().empty() ? 0 : score()->pages().front();
    }
    for (Page* page : score()->pages()) {
        mu::RectF pageBBox = page->bbox().translated(page->pos());
#if MSCOREVIEW_DEBUG
        printf("[MSCOREVIEW] point2page: page bbox(%.1f, %.1f, %.1f, %.1f), pos(%.1f, %.1f)\n",
               pageBBox.x(), pageBBox.y(), pageBBox.width(), pageBBox.height(),
               page->pos().x(), page->pos().y());
#endif
        if (pageBBox.contains(p)) {
#if MSCOREVIEW_DEBUG
            printf("[MSCOREVIEW] point2page: found page!\n");
#endif
            return page;
        }
    }
#if MSCOREVIEW_DEBUG
    printf("[MSCOREVIEW] point2page: no page found!\n");
#endif
    return 0;
}

//---------------------------------------------------------
//   elementsAt
//    p is in canvas coordinates
//---------------------------------------------------------

const std::vector<EngravingItem*> MuseScoreView::elementsAt(const mu::PointF& p) const
{
    std::vector<EngravingItem*> el;

    Page* page = point2page(p);
    if (page) {
        el = page->items(p - page->pos());
        std::sort(el.begin(), el.end(), elementLower);
    }

    return el;
}

EngravingItem* MuseScoreView::elementNear(const mu::PointF& pos) const
{
    std::vector<EngravingItem*> near = elementsNear(pos);
    if (near.empty()) {
        return nullptr;
    }
    return near.front();
}

const std::vector<EngravingItem*> MuseScoreView::elementsNear(const mu::PointF& pos) const
{
    std::vector<EngravingItem*> ll;
    Page* page = point2page(pos);
    if (!page) {
        return ll;
    }

    mu::PointF p = pos - page->pos();
    double w = selectionProximity();
    RectF r(p.x() - w, p.y() - w, 3.0 * w, 3.0 * w);

#if MSCOREVIEW_DEBUG
    printf("[MSCOREVIEW] elementsNear: pos(%.1f, %.1f), pagePos(%.1f, %.1f), localP(%.1f, %.1f)\n",
           pos.x(), pos.y(), page->pos().x(), page->pos().y(), p.x(), p.y());
    printf("[MSCOREVIEW] elementsNear: selectionProximity=%.1f, searchRect(%.1f, %.1f, %.1f, %.1f)\n",
           w, r.x(), r.y(), r.width(), r.height());
#endif

    std::vector<EngravingItem*> el = page->items(r);
#if MSCOREVIEW_DEBUG
    printf("[MSCOREVIEW] elementsNear: page->items(r) returned %zu elements\n", el.size());
#endif

    for (int i = 0; i < MAX_HEADERS; i++) {
        if (score()->headerText(i) != nullptr) {
            el.push_back(score()->headerText(i));
        }
    }
    for (int i = 0; i < MAX_FOOTERS; i++) {
        if (score()->footerText(i) != nullptr) {
            el.push_back(score()->footerText(i));
        }
    }
#if MSCOREVIEW_DEBUG
    printf("[MSCOREVIEW] elementsNear: total elements after headers/footers: %zu\n", el.size());
    for (size_t i = 0; i < el.size() && i < 10; i++) {
        EngravingItem* e = el[i];
        RectF eBbox = e->bbox();
        PointF ePos = e->pagePos();
        printf("[MSCOREVIEW]   [%zu] type=%s, selectable=%d, bbox(%.1f,%.1f,%.1f,%.1f), pagePos(%.1f,%.1f)\n",
               i, e->typeName(), e->selectable(),
               eBbox.x(), eBbox.y(), eBbox.width(), eBbox.height(),
               ePos.x(), ePos.y());
    }
#endif

    for (EngravingItem* e : el) {
        e->itemDiscovered = 0;
        if (!e->selectable() || e->isPage()) {
            continue;
        }
        if (e->contains(p)) {
#if MSCOREVIEW_DEBUG
            printf("[MSCOREVIEW] elementsNear: element %s CONTAINS point\n", e->typeName());
#endif
            ll.push_back(e);
        }
    }
    size_t n = ll.size();
#if MSCOREVIEW_DEBUG
    printf("[MSCOREVIEW] elementsNear: after contains check, found %zu elements\n", n);
#endif

    if ((n == 0) || ((n == 1) && (ll[0]->isMeasure()))) {
        //
        // if no relevant element hit, look nearby
        //
        for (EngravingItem* e : el) {
            if (e->isPage() || !e->selectable()) {
                continue;
            }
            if (e->intersects(r)) {
#if MSCOREVIEW_DEBUG
                printf("[MSCOREVIEW] elementsNear: element %s INTERSECTS rect\n", e->typeName());
#endif
                ll.push_back(e);
            }
        }
    }
#if MSCOREVIEW_DEBUG
    printf("[MSCOREVIEW] elementsNear: final result: %zu elements\n", ll.size());
#endif

    if (!ll.empty()) {
        std::sort(ll.begin(), ll.end(), elementLower);
    }
    return ll;
}
}
