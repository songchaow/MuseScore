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

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_set>

#include "draw/painter.h"
#include "libmscore/score.h"
#include "libmscore/page.h"
#include "libmscore/engravingitem.h"

#include "debugpaint.h"

#include "log.h"

#if MUSESCORE_PORTABLE_ENABLE_DRAW_DEBUG
#include "../../../../../drawdebug_logger.h"
#endif

// Repaint cost breakdown counters (portable build). These are plain integer
// increments, so they stay compiled in — the canvas surfaces them through
// GScoreCanvas::get_draw_stats() so a test can attribute repaint time to the
// page loop, the BSP query and the per-element draw loop separately.
namespace {
// Monotonic microsecond clock for the portable repaint counters.
int64_t portable_ticks_usec()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
}

struct PaintBreakdown {
    int pages_total = 0;
    int pages_culled = 0;
    int bsp_query_usec = 0;
    int draw_loop_usec = 0;
    int sort_usec = 0;
    int elements_drawn = 0;
    void reset() { *this = PaintBreakdown(); }
};
PaintBreakdown g_paint_breakdown;
}

namespace mu::engraving::rendering::dev {
const void* paint_breakdown_stats(int& pages_total, int& pages_culled,
                                  int& bsp_usec, int& draw_usec, int& sort_usec,
                                  int& elements_drawn)
{
    pages_total = g_paint_breakdown.pages_total;
    pages_culled = g_paint_breakdown.pages_culled;
    bsp_usec = g_paint_breakdown.bsp_query_usec;
    draw_usec = g_paint_breakdown.draw_loop_usec;
    sort_usec = g_paint_breakdown.sort_usec;
    elements_drawn = g_paint_breakdown.elements_drawn;
    return &g_paint_breakdown;
}
void reset_paint_breakdown() { g_paint_breakdown.reset(); }
}

using namespace mu::engraving;
using namespace mu::engraving::rendering::dev;

#if MUSESCORE_PORTABLE_ENABLE_DRAW_DEBUG
namespace {
constexpr size_t MONITORED_TYPE_SAMPLE_LIMIT = 3;

struct MonitoredTypeCounts {
    int page_total = 0;
    int frame_intersecting = 0;
    int candidate = 0;
};

struct MonitoredItemSample {
    const EngravingItem* item = nullptr;
    bool selected_by_candidate = false;
};

struct PageQueryTypeStats {
    MonitoredTypeCounts note;
    MonitoredTypeCounts stem;
    MonitoredTypeCounts barline;
};

struct PageQueryTypeSamples {
    std::vector<MonitoredItemSample> note;
    std::vector<MonitoredItemSample> stem;
    std::vector<MonitoredItemSample> barline;
};

std::string jsonEscape(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (char ch : value) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20) {
                escaped += ' ';
            } else {
                escaped += ch;
            }
            break;
        }
    }
    return escaped;
}

std::string quoted(const std::string& value)
{
    return std::string("\"") + jsonEscape(value) + "\"";
}

std::string pointToJson(const mu::PointF& point)
{
    std::ostringstream oss;
    oss << "{\"x\":" << point.x()
        << ",\"y\":" << point.y() << "}";
    return oss.str();
}

std::string rectToJson(const mu::RectF& rect)
{
    std::ostringstream oss;
    oss << "{\"x\":" << rect.x()
        << ",\"y\":" << rect.y()
        << ",\"width\":" << rect.width()
        << ",\"height\":" << rect.height() << "}";
    return oss.str();
}

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

std::vector<MonitoredItemSample>* monitoredTypeSamplesFor(PageQueryTypeSamples& samples, const EngravingItem* item)
{
    if (!item) {
        return nullptr;
    }

    switch (item->type()) {
    case ElementType::NOTE:
        return &samples.note;
    case ElementType::STEM:
        return &samples.stem;
    case ElementType::BAR_LINE:
        return &samples.barline;
    default:
        return nullptr;
    }
}

bool monitoredSampleLess(const MonitoredItemSample& lhs, const MonitoredItemSample& rhs)
{
    const mu::RectF lhsRect = lhs.item ? lhs.item->pageBoundingRect() : mu::RectF();
    const mu::RectF rhsRect = rhs.item ? rhs.item->pageBoundingRect() : mu::RectF();

    if (lhsRect.top() != rhsRect.top()) {
        return lhsRect.top() < rhsRect.top();
    }
    if (lhsRect.left() != rhsRect.left()) {
        return lhsRect.left() < rhsRect.left();
    }
    return reinterpret_cast<uintptr_t>(lhs.item) < reinterpret_cast<uintptr_t>(rhs.item);
}

void trimAndSortSamples(std::vector<MonitoredItemSample>& samples)
{
    std::sort(samples.begin(), samples.end(), monitoredSampleLess);
    if (samples.size() > MONITORED_TYPE_SAMPLE_LIMIT) {
        samples.resize(MONITORED_TYPE_SAMPLE_LIMIT);
    }
}

void collectMonitoredTypeSamples(PageQueryTypeSamples& samples,
                                 const std::vector<EngravingItem*>& page_items,
                                 const std::unordered_set<const EngravingItem*>& candidate_items)
{
    for (const EngravingItem* item : page_items) {
        std::vector<MonitoredItemSample>* bucket = monitoredTypeSamplesFor(samples, item);
        if (!bucket) {
            continue;
        }

        bucket->push_back(MonitoredItemSample {
            item,
            candidate_items.find(item) != candidate_items.end(),
        });
    }

    trimAndSortSamples(samples.note);
    trimAndSortSamples(samples.stem);
    trimAndSortSamples(samples.barline);
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

void appendMonitoredTypeSamples(std::ostringstream& oss,
                                const char* name,
                                const std::vector<MonitoredItemSample>& samples,
                                const mu::RectF& frame_local_rect,
                                bool add_comma)
{
    oss << "\"" << name << "\":[";
    for (size_t index = 0; index < samples.size(); ++index) {
        const MonitoredItemSample& sample = samples.at(index);
        const EngravingItem* item = sample.item;
        const mu::RectF pageBoundingRect = item ? item->pageBoundingRect() : mu::RectF();
        const bool intersectsFrame = item && (!frame_local_rect.isValid() || pageBoundingRect.intersects(frame_local_rect));

        oss << "{"
            << "\"element_ptr\":" << reinterpret_cast<uintptr_t>(item)
            << ",\"subtype\":" << quoted(item ? item->translatedSubtypeUserName().toStdString() : "")
            << ",\"page_pos\":" << pointToJson(item ? item->pagePos() : mu::PointF())
            << ",\"bbox\":" << rectToJson(item ? item->bbox() : mu::RectF())
            << ",\"page_bounding_rect\":" << rectToJson(pageBoundingRect)
            << ",\"intersects_frame_local_rect\":" << (intersectsFrame ? "true" : "false")
            << ",\"selected_by_bsp_candidate\":" << (sample.selected_by_candidate ? "true" : "false")
            << "}";

        if ((index + 1) < samples.size()) {
            oss << ",";
        }
    }
    oss << "]";
    if (add_comma) {
        oss << ",";
    }
}

std::string monitoredTypeSamplesToJson(const PageQueryTypeSamples& samples, const mu::RectF& frame_local_rect)
{
    std::ostringstream oss;
    oss << "{";
    appendMonitoredTypeSamples(oss, "Note", samples.note, frame_local_rect, true);
    appendMonitoredTypeSamples(oss, "Stem", samples.stem, frame_local_rect, true);
    appendMonitoredTypeSamples(oss, "BarLine", samples.barline, frame_local_rect, false);
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
            ++g_paint_breakdown.pages_total;
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
                // Pages are laid out along the Y axis in vertical orientation
                // (dev/pagelayout.cpp) and along X otherwise. The upstream
                // culling only ever compared X, so in vertical mode every
                // page survived and the loop ran a fillRect + clip + BSP
                // query per page for pages that are entirely off-screen.
                // Compare BOTH axes so the loop can `continue` past pages
                // above the viewport and `break` at the first page below it.
                if (pageAbsRect.right() < opt.frameRect.left()
                    || pageAbsRect.bottom() < opt.frameRect.top()) {
                    ++g_paint_breakdown.pages_culled;
                    continue;
                }

                if (pageAbsRect.left() > opt.frameRect.right()
                    || pageAbsRect.top() > opt.frameRect.bottom()) {
                    ++g_paint_breakdown.pages_culled;
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
            const int64_t t_bsp0 = portable_ticks_usec();
            std::vector<EngravingItem*> elements = page->items(frameLocalRect);
            g_paint_breakdown.bsp_query_usec += static_cast<int>(portable_ticks_usec() - t_bsp0);

#if MUSESCORE_PORTABLE_ENABLE_DRAW_DEBUG
            PageQueryTypeStats monitoredTypeStats;
            accumulatePageTypeStats(monitoredTypeStats, pageElements, frameLocalRect, false);
            accumulatePageTypeStats(monitoredTypeStats, elements, frameLocalRect, true);

            const std::unordered_set<const EngravingItem*> candidateItems(elements.begin(), elements.end());
            PageQueryTypeSamples monitoredTypeSamples;
            collectMonitoredTypeSamples(monitoredTypeSamples, pageElements, candidateItems);

            DrawDebugLogger::instance().logPageQuery(pi,
                                                     pageAbsRect,
                                                     opt.frameRect,
                                                     drawRect,
                                                     frameLocalRect,
                                                     pageElementCount,
                                                     static_cast<int>(elements.size()),
                                                     pageIntersectsFrame,
                                                     clippingRequested,
                                                     clippingRequested
                                                     ? "candidate_count_from_bsp_query; clipping_requested_but_backend_result_unknown"
                                                     : "candidate_count_from_bsp_query",
                                                     monitoredTypeCountsToJson(monitoredTypeStats),
                                                     monitoredTypeSamplesToJson(monitoredTypeSamples, frameLocalRect));
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
    const int64_t t_sort0 = portable_ticks_usec();
    std::vector<EngravingItem*> sortedItems(items.begin(), items.end());
    std::sort(sortedItems.begin(), sortedItems.end(), mu::engraving::elementLessThan);
    g_paint_breakdown.sort_usec += static_cast<int>(portable_ticks_usec() - t_sort0);

    const int64_t t_loop0 = portable_ticks_usec();
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
    g_paint_breakdown.draw_loop_usec += static_cast<int>(portable_ticks_usec() - t_loop0);
    g_paint_breakdown.elements_drawn += sortedIndex;

#ifdef MUE_ENABLE_ENGRAVING_PAINT_DEBUGGER
    if (!isPrinting) {
        DebugPaint::paintElementsDebug(painter, sortedItems);
    }
#else
    UNUSED(isPrinting);
#endif
}
