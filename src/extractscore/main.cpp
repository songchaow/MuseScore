#include "score.h"
#include "internal/masternotation.h"
#include "internal/mscznotationreader.h"
#include "libmscore/preferences.h"
#include "libmscore/musescoreCore.h"
#include "libmscore/trill.h"
#include "libmscore/tie.h"
#include "framework/fonts/fontsmodule.h"

#include "sequence.pb.h"
#include "aixlog.hpp"

#include <string>
#include <memory>
#include <set>
#include <map>
#include <unordered_map>

#include <QDebug>

using namespace Ms;

// unimplemented
std::map<std::string, std::string> paresArgs(int argc, char* argv[]) {
    std::map<std::string, std::string> ret;
    if (argc < 2)
        return std::map<std::string, std::string>();
    std::string last_option;
    bool positional = true;

    for (int i = 1; i <= argc - 1; i++) {
        std::string s(argv[i]);
        if (s.size() > 0 && s[0] == '-')
            last_option = s;
        else if (s.size() > 0) {
            if (!last_option.empty()) {
                ret[last_option] = s;
                last_option.clear();
            }
        }
    }
}

Trill* findFirstTrill(Ms::Chord* chord)
{
    auto spanners = chord->score()->spannerMap().findOverlapping(1 + chord->tick().ticks(),
        chord->tick().ticks() + chord->actualTicks().ticks()
        - 1);
    for (auto i : spanners) {
        if (i.value->type() != ElementType::TRILL) {
            continue;
        }
        if (i.value->track() != chord->track()) {
            continue;
        }
        Trill* trill = toTrill(i.value);
        if (trill->playArticulation() == false) {
            continue;
        }
        return trill;
    }
    return nullptr;
}

// instrument string prefix => canonical string, enum value
struct InstrumentInfo {
    std::string id;
    museprotocol::Note_Instrument enumval;
};
std::unordered_map<std::string, InstrumentInfo> infoMap = {
    {"wind.flutes",         {"wind.flutes.flute",       museprotocol::Note_Instrument::Note_Instrument_Flute}},
    {"wind.reed.oboe",      {"wind.reed.oboe",          museprotocol::Note_Instrument::Note_Instrument_Oboe}},
    {"wind.reed.clarinet",  {"wind.reed.clarinet",      museprotocol::Note_Instrument::Note_Instrument_Clarinet}},
    {"wind.reed.bassoon",   {"wind.reed.bassoon",       museprotocol::Note_Instrument::Note_Instrument_Basson}},
    {"brass.trumpet",       {"brass.trumpet",           museprotocol::Note_Instrument::Note_Instrument_Trumpet}},
    {"brass.french-horn",   {"brass.french-horn",       museprotocol::Note_Instrument::Note_Instrument_Horn}},
    {"brass.natural-horn",  {"brass.natural-horn",      museprotocol::Note_Instrument::Note_Instrument_Horn}},
    {"brass.sackbutt",      {"*",                       museprotocol::Note_Instrument::Note_Instrument_Trombone}},
    {"brass.trombone",      {"brass.trombone",          museprotocol::Note_Instrument::Note_Instrument_Trombone}},
    {"brass.tuba",          {"brass.tuba",              museprotocol::Note_Instrument::Note_Instrument_Tuba}},
    {"pluck.harp",          {"pluck.harp",              museprotocol::Note_Instrument::Note_Instrument_Harp}},
    //{""}
};

void fillInstrumentField(Note* n, Instrument* src, museprotocol::Note& note) {
    if (src->instrumentId() == "strings.group") {
        QString longname = src->longNames()[0].name();
        QString channelName = src->channel(n->subchannel())->name();
        if (longname == "Violoncellos") {
            if(channelName=="arco")
                note.set_instrument(museprotocol::Note_Instrument::Note_Instrument_Violoncellos);
            else if(channelName=="pizzicato")
                note.set_instrument(museprotocol::Note_Instrument::Note_Instrument_Violoncellos_pizz);
            else if(channelName=="tremolo")
                note.set_instrument(museprotocol::Note_Instrument::Note_Instrument_Violoncellos_trem);
        }
        else if (longname == "Violins") {
            if (channelName == "arco")
                note.set_instrument(museprotocol::Note_Instrument::Note_Instrument_Violins);
            else if (channelName == "pizzicato")
                note.set_instrument(museprotocol::Note_Instrument::Note_Instrument_Violins_pizz);
            else if (channelName == "tremolo")
                note.set_instrument(museprotocol::Note_Instrument::Note_Instrument_Violins_trem);
        }
        else if (longname == "Violas") {
            if (channelName == "arco")
                note.set_instrument(museprotocol::Note_Instrument::Note_Instrument_Violas);
            else if (channelName == "pizzicato")
                note.set_instrument(museprotocol::Note_Instrument::Note_Instrument_Violas_pizz);
            else if (channelName == "tremolo")
                note.set_instrument(museprotocol::Note_Instrument::Note_Instrument_Violas_trem);
        }
        else if (longname == "Contrabasses") {
            if (channelName == "arco")
                note.set_instrument(museprotocol::Note_Instrument::Note_Instrument_Contrabasses);
            else if (channelName == "pizzicato")
                note.set_instrument(museprotocol::Note_Instrument::Note_Instrument_Contrabasses_pizz);
            else if (channelName == "tremolo")
                note.set_instrument(museprotocol::Note_Instrument::Note_Instrument_Contrabasses_trem);
        }
        return;
    }
    QString instrID = src->instrumentId();
    if (instrID.isEmpty()) {
        std::cout << "Empty Instrument ID" << std::endl;
        return;
    }
    auto res = infoMap.find(instrID.toStdString());
    if (res == infoMap.end()) {
        std::string instrIDpre = instrID.toStdString();
        do {
            // shrink. remove the last point and following content
            int idxAfterPoint = instrIDpre.size();
            assert(idxAfterPoint > 0);
            while (idxAfterPoint > 0) {
                if (instrIDpre[idxAfterPoint - 1] == '.')
                    break;
                idxAfterPoint--;
            }
            instrIDpre = instrIDpre.substr(0, std::max(idxAfterPoint - 1, 0));
            res = infoMap.find(instrIDpre);
        } while (res == infoMap.end() && !instrIDpre.empty());
    }

    if (res == infoMap.end()) {
        std::cout << "Instrument" << instrID.toStdString() << "not found.";
        return;
    }
    
    else {
        // if exactly match
        if (res->second.id != "*" && res->second.id != instrID.toStdString()) {
            std::cout << "Instrument ID " << instrID.toStdString() << " does not exactly match. ";
            std::cout << "Result Str: " << res->second.id << std::endl;
        }
        note.set_instrument(res->second.enumval);
    }
}


int main(int argc, char* argv[]) {
    // parse args
    if (argc < 2) {
        std::cerr << "No score path provided." << std::endl;
        return 1;
    }
    if (argc >= 3)
        std::cout << "Warning: too many arguments." << std::endl;
    std::string score_path = argv[1];

    /*mu::notation::MasterNotation notation;
    notation.load(score_path);*/
    MScore::testMode = true;
    MScore::noGui = true;
    QApplication app(argc, argv);

    auto mscoreGlobal = Ms::MScore();
    new MuseScoreCore;
    // fonts
    //mu::fonts::init_fonts_qrc();
    mu::fonts::FontsModule fontmodule;
    fontmodule.registerResources();

    MScore::init();
    // another way
    std::shared_ptr<MasterScore> currscore = std::make_shared<MasterScore>(mscoreGlobal.baseStyle());
    mu::notation::MsczNotationReader reader;
    reader.read(currscore.get(), score_path);


    // iterate segments
    Ms::Measure* m = currscore->firstMeasure();
    while (m) {
        Ms::Segment* s = m->first(SegmentType::TimeSig);
        if (s) {
            // Time Signature, not necessarily used. Record at most one signatrue here

        }
        


        m = m->nextMeasure();
    }
    m->first();
    Ms::Segment* s = currscore->firstSegment(SegmentType::All);
    const std::set<SegmentType> exclude = {

    };
    while (s) {
        // only ChordRest?
        if (s->segmentType() == SegmentType::ChordRest) {
            const std::vector<Element*>& elements = s->elist();
            for (Element* e : elements) {
                ChordRest* cr = static_cast<ChordRest*>(e);
                if (cr && cr->isChord()) {
                    Ms::Chord* chord = static_cast<Ms::Chord*>(cr);
                    if (chord->crossMeasure() == CrossMeasure::SECOND) {
                        // already counted in FIRST
                        continue;
                    }
                    // instrument
                    Instrument* instr = chord->part()->instrument(chord->tick());
                    QString instrID = instr->instrumentId();
                    
                    // trill
                    Trill* tr = findFirstTrill(chord);
                    if (tr) {
                        // is lasting
                        ;
                    }
                    // volume
                    int tickpos = chord->tick().ticks();
                    int chordTickLen = chord->actualTicks().ticks();
                    int ondelayticks = chordTickLen * chord->notes()[0]->playEvents()[0].ontime() / 1000; // delay of the NoteEvent from the chord starting tick
                    
                    int volume = chord->staff()->velocities().val(Fraction::fromTicks(tickpos + ondelayticks));
                    for (Ms::Note* n : chord->notes()) {
                        // duration
                        Fraction duration = chord->durationType().fraction();
                        // if it's tied to previous notes, skip it
                        if (n->tieBack())
                            continue;
                        if (n->tieFor()) {
                            // find all after notes and sum the duration

                            Note* currNote = n;
                            while (currNote->tieFor()) {
                                Note* nextNote = static_cast<Note*>(currNote->tieFor()->endElement());
                                assert(nextNote->pitch() == n->pitch());
                                // add nextNote's duration
                                if (!nextNote)
                                    break;
                                duration += nextNote->chord()->durationType().fraction();
                                currNote = nextNote;
                            }
                        }
                        museprotocol::Note note;
                        note.set_duration(duration.ticks());
                        note.set_pitch(n->pitch()); // 0-127
                        auto& spanners = n->spannerBack();
                        int size1 = spanners.size();
                        auto& spanners2 = n->spannerFor();
                        int size2 = spanners2.size();
                        // Instrument
                        fillInstrumentField(n, instr, note);
                    }
                }
            }
        }
        // exclude 
        s = s->next();
    }



    return 0;
}