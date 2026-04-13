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
#include "smufl.h"

#include <memory>

#include "io/file.h"
#include "serialization/json.h"

#include "types/symnames.h"

#include "libmscore/mscore.h"

#include "log.h"

namespace {
std::unique_ptr<mu::io::File> openReadOnlyResourceFileWithFallback(const mu::io::path_t& path, mu::io::path_t* resolvedPath = nullptr)
{
    auto tryOpen = [](const mu::io::path_t& candidate) -> std::unique_ptr<mu::io::File> {
        auto file = std::make_unique<mu::io::File>(candidate);
        if (!file->open(mu::io::IODevice::ReadOnly)) {
            return nullptr;
        }
        return file;
    };

    if (auto file = tryOpen(path)) {
        if (resolvedPath) {
            *resolvedPath = path;
        }
        return file;
    }

    if (path.toStdString().rfind("res://", 0) != 0) {
        const mu::io::path_t resPath = "res://" + path;
        if (auto file = tryOpen(resPath)) {
            if (resolvedPath) {
                *resolvedPath = resPath;
            }
            return file;
        }
    }

    return nullptr;
}
}

using namespace mu;
using namespace mu::io;
using namespace mu::engraving;

std::array<Smufl::Code, size_t(SymId::lastSym) + 1> Smufl::s_symIdCodes { {  } };

const Smufl::Code Smufl::code(SymId id)
{
    return s_symIdCodes.at(static_cast<size_t>(id));
}

char32_t Smufl::smuflCode(SymId id)
{
    return s_symIdCodes.at(static_cast<size_t>(id)).smuflCode;
}

bool Smufl::init()
{
    bool ok = initGlyphNamesJson();

    return ok;
}

bool Smufl::initGlyphNamesJson()
{
    io::path_t glyphNamesPath("fonts/smufl/glyphnames.json");
    std::unique_ptr<File> file = openReadOnlyResourceFileWithFallback(glyphNamesPath, &glyphNamesPath);
    if (!file) {
        LOGE() << "could not open glyph names JSON file. attempted path: " << glyphNamesPath;
        return false;
    }

    std::string error;
    JsonObject glyphNamesJson = JsonDocument::fromJson(file->readAll(), &error).rootObject();
    file->close();

    if (!error.empty()) {
        LOGE() << "JSON parse error in glyph names file: " << error;
        return false;
    }

    IF_ASSERT_FAILED(!glyphNamesJson.empty()) {
        LOGE() << "Could not read glyph names JSON";
        return false;
    }

    for (size_t i = 0; i < s_symIdCodes.size(); ++i) {
        SymId sym = static_cast<SymId>(i);
        if (sym == SymId::noSym || sym == SymId::lastSym) {
            continue;
        }

        std::string name(SymNames::nameForSymId(sym).ascii());
        JsonObject symObj = glyphNamesJson.value(name).toObject();
        if (!symObj.isValid()) {
            continue;
        }

        bool ok;
        char32_t code = symObj.value("codepoint").toString().mid(2).toUInt(&ok, 16);
        if (ok) {
            s_symIdCodes[i].smuflCode = code;
        } else if (MScore::debugMode) {
            LOGD() << "could not read codepoint for glyph " << name;
        }

        char32_t alernativeCode = symObj.value("alternateCodepoint").toString().mid(2).toUInt(&ok, 16);
        if (ok) {
            s_symIdCodes[i].musicSymBlockCode = alernativeCode;
        } else if (MScore::debugMode) {
            LOGD() << "could not read alternate codepoint for glyph " << name;
        }
    }
    return true;
}

//---------------------------------------------------------
//   smuflRanges
//    read smufl ranges.json file
//---------------------------------------------------------

const std::map<String, StringList>& Smufl::smuflRanges()
{
    static std::map<String, StringList> ranges;
    StringList allSymbols;

    if (ranges.empty()) {
        io::path_t rangesPath("fonts/smufl/ranges.json");
        std::unique_ptr<File> fi = openReadOnlyResourceFileWithFallback(rangesPath, &rangesPath);
        if (!fi) {
            LOGE() << "failed open: " << rangesPath;
        }
        std::string error;
        JsonObject o = fi ? JsonDocument::fromJson(fi->readAll(), &error).rootObject() : JsonObject();
        if (!error.empty()) {
            LOGE() << "failed parse, err: " << error << ", file: " << rangesPath;
        }

        for (auto s : o.keys()) {
            JsonObject range = o.value(s).toObject();
            String desc      = range.value("description").toString();
            JsonArray glyphs = range.value("glyphs").toArray();
            if (glyphs.size() > 0) {
                StringList glyphNames;
                for (size_t i = 0; i < glyphs.size(); ++i) {
                    glyphNames.append(glyphs.at(i).toString());
                }
                ranges.insert({ desc, glyphNames });
                allSymbols << glyphNames;
            }
        }
        ranges.insert({ String::fromAscii(SMUFL_ALL_SYMBOLS), allSymbols });     // TODO: make translatable as well as ranges.json
    }
    return ranges;
}
