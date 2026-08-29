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
#ifndef MU_ENGRAVING_PAINT_H
#define MU_ENGRAVING_PAINT_H

#include <vector>

#include "draw/painter.h"
#include "../iscorerenderer.h"

namespace mu::engraving {
class EngravingItem;
class Page;
class Score;
}

namespace mu::engraving::rendering::dev {
class Paint
{
public:

    static void paintScore(draw::Painter* painter, Score* score, const IScoreRenderer::PaintOptions& opt);
    static void paintItem(draw::Painter& painter, const EngravingItem* item, int pageIndex = -1, int sortedIndex = -1);
    static void paintItems(draw::Painter& painter, const std::vector<EngravingItem*>& items, bool isPrinting, int pageIndex = -1);

    static SizeF pageSizeInch(const Score* score);
    static SizeF pageSizeInch(const Score* score, const IScoreRenderer::PaintOptions& opt);

private:
};

// Repaint cost breakdown (portable instrumentation). Lets the Godot canvas
// attribute repaint time to the page loop, BSP query, sort and draw loop.
const void* paint_breakdown_stats(int& pages_total, int& pages_culled,
                                  int& bsp_usec, int& draw_usec, int& sort_usec,
                                  int& elements_drawn);
void reset_paint_breakdown();
}

#endif // MU_ENGRAVING_PAINT_H
