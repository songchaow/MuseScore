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
#include "paint.h"

#include <sstream>
#include <string>

#include "draw/painter.h"
#include "libmscore/score.h"
#include "libmscore/page.h"
#include "libmscore/engravingitem.h"

#include "debugpaint.h"

#include "log.h"

#if MUSESCORE_PORTABLE_ENABLE_DRAW_DEBUG
#include "../../../../../drawdebug_logger.h"
#endif

using namespace mu::engraving;
using namespace mu::engraving::rendering::dev;

#if MUSESCORE_PORTABLE_ENABLE_DRAW_DEBUG
namespace {
struct MonitoredTypeCounts {
    int page_total = 0;
    int frame_intersecting = 0;
    int candidate = 0;
};

struct PageQueryTypeStats {
    MonitoredTypeCounts note;
    MonitoredTypeCounts stem;
    MonitoredTypeCounts barline;
};

const char* interactionUnavailableNotes(const EngravingItem* item)
{
    if (!item) {
        return "missing_item";
    }

    const Score* score = item->score();
    if (!item->visible() && score && (score->printing() || !score->isShowInvisible())) {
        return "visible_false_hidden_by_score_visibility_settings";
    }

    return "isInteractionAvailable_returned_false";
}

MonitoredTypeCounts* monitoredTypeCountsFor(PageQueryTypeStats& stats, const EngravingItem* item)
{
    if (!item) {
        return nullptr;
    }

    switch (item->type()) {
    case ElementType::NOTE:
        return &stats.note;
    case ElementType::STEM:
        return &stats.stem;
    case ElementType::BAR_LINE:
        return &stats.barline;
    default:
        return nullptr;
    }
}

void accumulatePageTypeStats(PageQueryTypeStats& stats,
                             const std::vector<EngravingItem*>& items,
                             const mu::RectF& frame_local_rect,
                             bool treat_as_candidates)
{
    for (const EngravingItem* item : items) {
        MonitoredTypeCounts* counts = monitoredTypeCountsFor(stats, item);
        if (!counts) {
            continue;
        }

        if (treat_as_candidates) {
            counts->candidate += 1;
            continue;
        }

        counts->page_total += 1;
        if (!frame_local_rect.isValid() || item->pageBoundingRect().intersects(frame_local_rect)) {
            counts->frame_intersecting += 1;
        }
    }
}

std::string monitoredTypeCountsToJson(const PageQueryTypeStats& stats)
{
    auto appendCounts = [](std::ostringstream& oss, const char* name, const MonitoredTypeCounts& counts, bool add_comma) {
        oss << "\"" << name << "\":{"
            << "\"page_total\":" << counts.page_total
            << ",\"frame_intersecting\":" << counts.frame_intersecting
            << ",\"candidate\":" << counts.candidate
            << "}";
        if (add_comma) {
            oss << ",";
        }
    };

    std::ostringstream oss;
    oss << "{";
    appendCounts(oss, "Note", stats.note, true);
    appendCounts(oss, "Stem", stats.stem, true);
    appendCounts(oss, "BarLine", stats.barline, false);
    oss << "}";
    return oss.str();
}
}
#endif

void Paint::paintScore(draw::Painter* painter, Score* score, const IScoreRenderer::PaintOptions& opt)
{
    TRACEFUNC;
    if (!score) {
        return;
    }

    const std::vector<Page*>& pages = score->pages();
    if (pages.empty()) {
        return;
    }

    //! NOTE This is DPI of paint device,  ex screen, image, printer and etc.
    //! Should be set, but if not set, we will use our default DPI.
    const int DEVICE_DPI = opt.deviceDpi > 0 ? opt.deviceDpi : mu::engraving::DPI;

    //! NOTE Depending on the view mode,
    //! if the view mode is PAGE, then this is one page size (ex A4),
    //! if someone a continuous mode, then this is the size of the entire score.
    SizeF pageSize = pageSizeInch(score);

    // Setup Painter
    painter->setAntialiasing(true);

    //! NOTE To draw on the screen, no need to adjust the viewport,
    //! to draw on others (pdf, png, printer), we need to set the viewport
    if (opt.isSetViewport) {
        painter->setViewport(RectF(0.0, 0.0, std::lrint(pageSize.width() * DEVICE_DPI), std::lrint(pageSize.height() * DEVICE_DPI)));
        painter->setWindow(RectF(0.0, 0.0, std::lrint(pageSize.width() * engraving::DPI), std::lrint(pageSize.height() * engraving::DPI)));
    }

    // Setup score draw system
    mu::engraving::MScore::pixelRatio = mu::engraving::DPI / DEVICE_DPI;
    score->setPrinting(opt.isPrinting);
    mu::engraving::MScore::pdfPrinting = opt.isPrinting;

    // Setup page counts
    int fromPage = opt.fromPage >= 0 ? opt.fromPage : 0;
    int toPage = (opt.toPage >= 0 && opt.toPage < int(pages.size())) ? opt.toPage : (int(pages.size()) - 1);

    for (int copy = 0; copy < opt.copyCount; ++copy) {
        bool firstPage = true;
        for (int pi = fromPage; pi <= toPage; ++pi) {
            Page* page = pages.at(pi);

            PointF pagePos = page->pos();
            RectF pageRect = page->bbox();

            //! NOTE Trim page margins, if need
            if (opt.trimMarginPixelSize >= 0) {
                double trimMargin = static_cast<double>(opt.trimMarginPixelSize);
                pageRect = page->tbbox().adjusted(-trimMargin, -trimMargin, trimMargin, trimMargin);
            }

            //! NOTE Check draw rect, usually for optimisation drawing on screen (draw only what we see)
            RectF drawRect;
            RectF pageAbsRect = pageRect.translated(pagePos);
            bool pageIntersectsFrame = true;
#if MUSESCORE_PORTABLE_ENABLE_DRAW_DEBUG
            const std::vector<EngravingItem*> pageElements = page->elements();
            const int pageElementCount = static_cast<int>(pageElements.size());
#endif
            if (opt.frameRect.isValid()) {
                if (pageAbsRect.right() < opt.frameRect.left()) {
#if MUSESCORE_PORTABLE_ENABLE_DRAW_DEBUG
                    DrawDebugLogger::instance().logPageQuery(pi,
                                                             pageAbsRect,
                                                             opt.frameRect,
                                                             RectF(),
                                                             pageElementCount,
                                                             0,
                                                             false,
                                                             false,
                                                             "page_before_frame_rect_left_edge");
#endif
                    continue;
                }

                if (pageAbsRect.left() > opt.frameRect.right()) {
#if MUSESCORE_PORTABLE_ENABLE_DRAW_DEBUG
                    DrawDebugLogger::instance().logPageQuery(pi,
                                                             pageAbsRect,
                                                             opt.frameRect,
                                                             RectF(),
                                                             pageElementCount,
                                                             0,
                                                             false,
                                                             false,
                                                             "page_after_frame_rect_right_edge");
#endif
                    break;
                }

                drawRect = opt.frameRect;
            } else {
                drawRect = pageAbsRect;
            }

            //! NOTE Notify about new page (usually for paged paint device, ex pdf, printer)
            if (!firstPage) {
                if (opt.onNewPage) {
                    opt.onNewPage();
                }
            }
            firstPage = false;

            painter->beginObject("page_" + std::to_string(pi));

            if (opt.isMultiPage) {
                painter->translate(pagePos);
            } else if (opt.trimMarginPixelSize >= 0) {
                painter->translate(-pageRect.topLeft());
            }

            // Draw page sheet
            if (opt.onPaintPageSheet) {
                opt.onPaintPageSheet(painter, page, pageRect);
            } else if (opt.printPageBackground) {
                painter->fillRect(pageRect, Color::WHITE);
            }

            // Draw page elements
            bool disableClipping = false;
            const bool clippingRequested = !painter->hasClipping();

            if (!painter->hasClipping()) {
                painter->setClipping(true);
                painter->setClipRect(pageRect);
                disableClipping = true;
            }

            const RectF frameLocalRect = drawRect.translated(-pagePos);
            std::vector<EngravingItem*> elements = page->items(frameLocalRect);

#if MUSESCORE_PORTABLE_ENABLE_DRAW_DEBUG
            PageQueryTypeStats monitoredTypeStats;
            accumulatePageTypeStats(monitoredTypeStats, pageElements, frameLocalRect, false);
            accumulatePageTypeStats(monitoredTypeStats, elements, frameLocalRect, true);

            DrawDebugLogger::instance().logPageQuery(pi,
                                                     pageAbsRect,
                                                     opt.frameRect,
                                                     drawRect,
                                                     pageElementCount,
                                                     static_cast<int>(elements.size()),
                                                     pageIntersectsFrame,
                                                     clippingRequested,
                                                     clippingRequested
                                                     ? "candidate_count_from_bsp_query; clipping_requested_but_backend_result_unknown"
                                                     : "candidate_count_from_bsp_query",
                                                     monitoredTypeCountsToJson(monitoredTypeStats));
#endif

            paintItems(*painter, elements, opt.isPrinting, pi);

            if (disableClipping) {
                painter->setClipping(false);
            }

#ifdef MUE_ENABLE_ENGRAVING_PAINT_DEBUGGER
            if (!opt.isPrinting) {
                DebugPaint::paintPageDebug(*painter, page);
            }
#endif

            painter->endObject(); // page

            if (opt.isMultiPage) {
                painter->translate(-pagePos);
            } else if (opt.trimMarginPixelSize >= 0) {
                painter->translate(pageRect.topLeft());
            }

            if ((copy + 1) < opt.copyCount) {
                //! NOTE Notify about new page (usually for paged paint device, ex pdf, printer)
                //! for next copy
                if (opt.onNewPage) {
                    opt.onNewPage();
                }
            }
        }
    }
}

SizeF Paint::pageSizeInch(const Score* score)
{
    if (!score) {
        return SizeF();
    }

    //! NOTE If now it is not PAGE view mode,
    //! then the page sizes will differ from the standard sizes (in PAGE view mode)
    if (score->npages() > 0) {
        const Page* page = score->pages().front();
        return SizeF(page->bbox().width() / mu::engraving::DPI, page->bbox().height() / mu::engraving::DPI);
    }

    return SizeF(score->style().styleD(Sid::pageWidth), score->style().styleD(Sid::pageHeight));
}

SizeF Paint::pageSizeInch(const Score* score, const IScoreRenderer::PaintOptions& opt)
{
    if (!score) {
        return SizeF();
    }

    int pageNo = opt.fromPage >= 0 ? opt.fromPage : 0;
    if (pageNo >= int(score->npages())) {
        return SizeF();
    }

    const Page* page = score->pages().at(pageNo);

    RectF pageRect = page->bbox();

    //! NOTE Trim page margins, if need
    if (opt.trimMarginPixelSize >= 0) {
        double trimMargin = static_cast<double>(opt.trimMarginPixelSize);
        pageRect = page->tbbox().adjusted(-trimMargin, -trimMargin, trimMargin, trimMargin);
    }

    return pageRect.size() / mu::engraving::DPI;
}

void Paint::paintItem(mu::draw::Painter& painter, const EngravingItem* item, int pageIndex, int sortedIndex)
{
    TRACEFUNC;
    if (item->skipDraw()) {
#if MUSESCORE_PORTABLE_ENABLE_DRAW_DEBUG
        DrawDebugLogger::instance().logElementState(item,
                                                    pageIndex,
                                                    sortedIndex,
                                                    true,
                                                    "paint_item",
                                                    "skip_draw",
                                                    "item->skipDraw() returned true");
#endif
        return;
    }
    item->itemDiscovered = false;
    PointF itemPosition(item->pagePos());

#if MUSESCORE_PORTABLE_ENABLE_DRAW_DEBUG
    DrawDebugLogger::instance().logElementState(item,
                                                pageIndex,
                                                sortedIndex,
                                                true,
                                                "paint_item",
                                                "dispatched_to_renderer",
                                                "paintItem translated item->pagePos() and called renderer()->drawItem()");
    DrawDebugElementScope elementScope(item, pageIndex, sortedIndex);
#endif

    painter.translate(itemPosition);
    EngravingItem::renderer()->drawItem(item, &painter);
    painter.translate(-itemPosition);
}

void Paint::paintItems(mu::draw::Painter& painter, const std::vector<EngravingItem*>& items, bool isPrinting, int pageIndex)
{
    TRACEFUNC;
    std::vector<EngravingItem*> sortedItems(items.begin(), items.end());

    std::sort(sortedItems.begin(), sortedItems.end(), mu::engraving::elementLessThan);

    int sortedIndex = 0;
    for (const EngravingItem* item : sortedItems) {
#if MUSESCORE_PORTABLE_ENABLE_DRAW_DEBUG
        DrawDebugLogger::instance().logElementState(item,
                                                    pageIndex,
                                                    sortedIndex,
                                                    true,
                                                    "paint_items_filter",
                                                    "sorted_candidate",
                                                    "item selected by page->items() BSP query and sorted by elementLessThan");
#endif
        if (!item->isInteractionAvailable()) {
#if MUSESCORE_PORTABLE_ENABLE_DRAW_DEBUG
            DrawDebugLogger::instance().logElementState(item,
                                                        pageIndex,
                                                        sortedIndex,
                                                        true,
                                                        "paint_items_filter",
                                                        "interaction_unavailable",
                                                        interactionUnavailableNotes(item));
#endif
            sortedIndex += 1;
            continue;
        }

        paintItem(painter, item, pageIndex, sortedIndex);
        sortedIndex += 1;
    }

#ifdef MUE_ENABLE_ENGRAVING_PAINT_DEBUGGER
    if (!isPrinting) {
        DebugPaint::paintElementsDebug(painter, sortedItems);
    }
#else
    UNUSED(isPrinting);
#endif
}
