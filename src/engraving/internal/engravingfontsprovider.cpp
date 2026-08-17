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

#include "engravingfontsprovider.h"

#include "global/stringutils.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

void EngravingFontsProvider::addFont(const std::string& name, const std::string& family,
                                     const io::path_t& fontPath, const io::path_t& metadataPath)
{
    std::shared_ptr<EngravingFont> f = std::make_shared<EngravingFont>(name, family, fontPath, metadataPath);
    m_symbolFonts.push_back(f);
    m_fallback.font = nullptr;
}

std::shared_ptr<EngravingFont> EngravingFontsProvider::doFontByName(const std::string& name) const
{
    std::string name_lo = mu::strings::toLower(name);
    for (const std::shared_ptr<EngravingFont>& f : m_symbolFonts) {
        if (mu::strings::toLower(f->name()) == name_lo) {
            return f;
        }
    }
    return nullptr;
}

IEngravingFontPtr EngravingFontsProvider::fontByName(const std::string& name) const
{
    std::shared_ptr<EngravingFont> font = doFontByName(name);
    if (!font) {
        font = doFallbackFont();
    }

    if (!font) {
        LOGE() << "Failed to find font: " << name << " and fallback font is also unavailable";
        return nullptr;
    }

    font->ensureLoad();
    return font;
}

std::vector<IEngravingFontPtr> EngravingFontsProvider::fonts() const
{
    std::vector<IEngravingFontPtr> fs;
    for (const std::shared_ptr<EngravingFont>& f : m_symbolFonts) {
        fs.push_back(f);
    }
    return fs;
}

void EngravingFontsProvider::setFallbackFont(const std::string& name)
{
    m_fallback.name = name;
    m_fallback.font = nullptr;
}

std::shared_ptr<EngravingFont> EngravingFontsProvider::doFallbackFont() const
{
    if (!m_fallback.font) {
        m_fallback.font = doFontByName(m_fallback.name);
        IF_ASSERT_FAILED(m_fallback.font) {
            return nullptr;
        }
    }

    return m_fallback.font;
}

IEngravingFontPtr EngravingFontsProvider::fallbackFont() const
{
    std::shared_ptr<EngravingFont> font = doFallbackFont();
    if (!font->loaded()) {
        LOGW() << "fallback font not loaded, symbols may be missing: " << font->name();
    }
    return font;
}

bool EngravingFontsProvider::isFallbackFont(const IEngravingFont* f) const
{
    return doFallbackFont().get() == f;
}

void EngravingFontsProvider::loadAllFonts()
{
    // Some symbol-font metadata resolves composed glyphs through the fallback
    // font. Load that dependency first so initialization never observes a
    // temporarily unavailable fallback.
    if (std::shared_ptr<EngravingFont> fallback = doFallbackFont()) {
        fallback->ensureLoad();
    }

    for (std::shared_ptr<EngravingFont>& f : m_symbolFonts) {
        f->ensureLoad();
    }
}
