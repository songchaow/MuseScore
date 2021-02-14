#include "score.h"
#include "internal/masternotation.h"
#include "internal/mscznotationreader.h"
#include "libmscore/preferences.h"
#include "libmscore/musescoreCore.h"
#include "libmscore/trill.h"
#include "libmscore/tie.h"
#include "framework/fonts/fontsmodule.h"
#include "framework/midi_old/exportmidi.h"
#include "utils.h"

#include "sequence.pb.h"
#include "aixlog.hpp"

#include <string>
#include <memory>
#include <set>
#include <map>
#include <unordered_map>
#include <fstream>
#include <filesystem>

#include <QDebug>

using namespace Ms;
namespace fs = std::filesystem;


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

struct VolumeReference {
    museprotocol::Note_Volume enumvalue;
    int vol;
};
VolumeReference volRefs[8] = {
    {museprotocol::Note_Volume::Note_Volume_PPP, 16},
    {museprotocol::Note_Volume::Note_Volume_PP, 33},
    {museprotocol::Note_Volume::Note_Volume_P, 49},
    {museprotocol::Note_Volume::Note_Volume_MP, 64},
    {museprotocol::Note_Volume::Note_Volume_MF, 80},
    {museprotocol::Note_Volume::Note_Volume_F, 96},
    {museprotocol::Note_Volume::Note_Volume_FF, 112},
    {museprotocol::Note_Volume::Note_Volume_FFF, 126}
};
museprotocol::Note_Volume nearestVolumeEnum(int vol) {
    int dist = std::abs(vol - volRefs[0].vol);
    museprotocol::Note_Volume ret = volRefs[0].enumvalue;
    for (int i = 0; i < 8; i++) {
        if (std::abs(volRefs[i].vol - vol) < dist) {
            ret = volRefs[i].enumvalue;
        }
    }
    return ret;
}

void fillInstrumentField(Note* n, Instrument* src, museprotocol::Note* note) {
    if (src->instrumentId() == "strings.group") {
        QString longname = src->longNames()[0].name();
        QString channelName = src->channel(n->subchannel())->name();
        if (longname == "Violoncellos") {
            if(channelName=="arco")
                note->set_instrument(museprotocol::Note_Instrument::Note_Instrument_Violoncellos);
            else if(channelName=="pizzicato")
                note->set_instrument(museprotocol::Note_Instrument::Note_Instrument_Violoncellos_pizz);
            else if(channelName=="tremolo")
                note->set_instrument(museprotocol::Note_Instrument::Note_Instrument_Violoncellos_trem);
        }
        else if (longname == "Violins") {
            if (channelName == "arco")
                note->set_instrument(museprotocol::Note_Instrument::Note_Instrument_Violins);
            else if (channelName == "pizzicato")
                note->set_instrument(museprotocol::Note_Instrument::Note_Instrument_Violins_pizz);
            else if (channelName == "tremolo")
                note->set_instrument(museprotocol::Note_Instrument::Note_Instrument_Violins_trem);
        }
        else if (longname == "Violas") {
            if (channelName == "arco")
                note->set_instrument(museprotocol::Note_Instrument::Note_Instrument_Violas);
            else if (channelName == "pizzicato")
                note->set_instrument(museprotocol::Note_Instrument::Note_Instrument_Violas_pizz);
            else if (channelName == "tremolo")
                note->set_instrument(museprotocol::Note_Instrument::Note_Instrument_Violas_trem);
        }
        else if (longname == "Contrabasses") {
            if (channelName == "arco")
                note->set_instrument(museprotocol::Note_Instrument::Note_Instrument_Contrabasses);
            else if (channelName == "pizzicato")
                note->set_instrument(museprotocol::Note_Instrument::Note_Instrument_Contrabasses_pizz);
            else if (channelName == "tremolo")
                note->set_instrument(museprotocol::Note_Instrument::Note_Instrument_Contrabasses_trem);
        }
        return;
    }
    QString instrID = src->instrumentId();
    if (instrID.isEmpty()) {
        std::cout << "Empty Instrument ID" << std::endl;
        note->set_instrument(museprotocol::Note_Instrument::Note_Instrument_Unknown);
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
        std::cout << "Instrument " << instrID.toStdString() << " not found." << std::endl;
        return;
    }
    
    else {
        // if exactly match
        if (res->second.id != "*" && res->second.id != instrID.toStdString()) {
            std::cout << "Instrument ID " << instrID.toStdString() << " does not exactly match. ";
            std::cout << "Result Str: " << res->second.id << std::endl;
        }
        note->set_instrument(res->second.enumval);
    }
}

museprotocol::Segment_VolumeChange calcVolumeChange(Ms::Chord* c) {
    static const int NUM_SAMPLE = 10;
    Staff* staff = c->staff();
    ChangeMap& velocities = staff->velocities();
    Fraction starttick = c->tick();
    Fraction increment = c->actualTicks() / NUM_SAMPLE;
    int increaseNum = 0;
    int decreaseNum = 0;
    for (int i = 0; i < NUM_SAMPLE-1; i++) {
        Fraction currTick = starttick + i * increment;
        Fraction nextTick = starttick + (i + 1) * increment;
        int currvol = velocities.val(currTick);
        int nextvol = velocities.val(nextTick);
        if (nextvol > currvol)
            increaseNum++;
        if (nextvol < currvol)
            decreaseNum++;
    }
    if (increaseNum > NUM_SAMPLE / 2)
        return museprotocol::Segment_VolumeChange::Segment_VolumeChange_Crescendo;
    if(decreaseNum > NUM_SAMPLE / 2)
        return museprotocol::Segment_VolumeChange::Segment_VolumeChange_Dim;
    return museprotocol::Segment_VolumeChange::Segment_VolumeChange_None;
}

bool findRamp(const ChangeMap& velocities, Fraction tick, ChangeEvent& ramp) {
    auto eventIter = velocities.upperBound(tick);
    ChangeEvent ret;
    if (eventIter == velocities.begin())
    {
        return false;
    }
    eventIter--;
    //ChangeEvent& rampFound = eventIter.value();         // only used to init
    Fraction rampFoundStartTick = eventIter.key();
    for (auto& event : velocities.values(rampFoundStartTick)) {
        if (event.eventType() == ChangeEventType::RAMP) {

            ramp = event;
            return true;
        }
    }
    return false;
}


void extractSegments(Ms::Score* currscore, museprotocol::Score& score) {
    // iterate segments
        Ms::Measure* m = currscore->firstMeasure();
        Fraction currTimeSig; // same for at least one measure
        int measureIdx = 0;


        while (m) {
            measureIdx++;
            Ms::Segment* s = m->first(SegmentType::TimeSig);
            if (s) {
                // Time Signature, not necessarily used. Record at most one signature here
                TimeSig* timesig = static_cast<TimeSig*>(s->element(0));
                currTimeSig = timesig->sig();
            }

            // ChordRest segments
            s = m->first(SegmentType::All);
            const std::set<SegmentType> exclude = {};
            while (s) {
                // only ChordRest?
                if (s->segmentType() == SegmentType::ChordRest) {
                    museprotocol::Segment* currSegment = score.add_segs();
                    //museprotocol::Segment currSegment;
                    museprotocol::Segment_VolumeChange volumeChange_seg = museprotocol::Segment_VolumeChange_None;
                    const std::vector<Element*>& elements = s->elist();
                    currSegment->set_pbar(measureIdx);
                    Fraction offsetBar = s->rtick() * currTimeSig.numerator() * 32 / currTimeSig.denominator();
                    int offsetBarInt = offsetBar.numerator() / offsetBar.denominator();
                    currSegment->set_poffset(offsetBarInt);
                    for (int i = 0; i < elements.size(); i++) {
                        auto e = elements[i];
                        ChordRest* cr = static_cast<ChordRest*>(e);
                        if (cr && cr->isChord()) {
                            Ms::Chord* chord = static_cast<Ms::Chord*>(cr);
                            if (chord->crossMeasure() == CrossMeasure::SECOND) {
                                // already counted in FIRST
                                continue;
                            }
                            // instrument
                            Instrument* instr = chord->part()->instrument(chord->tick());
                            //QString instrID = instr->instrumentId();

                            // trill
                            Trill* tr = findFirstTrill(chord);
                            if (tr) {
                                // is lasting
                                ;
                            }
                            // volume (same for all notes in a chord)
                            int tickpos = chord->tick().ticks();
                            int chordTickLen = chord->actualTicks().ticks();
                            int ondelayticks = chordTickLen * chord->notes()[0]->playEvents()[0].ontime() / 1000; // delay of the NoteEvent from the chord starting tick
                            ChangeMap& veloEvents = chord->staff()->velocities();
                            ChangeMap& veloMulEvents = chord->staff()->velocityMultiplications();
                            int volume = chord->staff()->velocities().val(Fraction::fromTicks(tickpos + ondelayticks));
                            museprotocol::Note_Volume vol_enum = nearestVolumeEnum(volume);
                            // volume change for segment
                            museprotocol::Segment_VolumeChange currrVolChange = calcVolumeChange(chord);
                            if (currrVolChange != museprotocol::Segment_VolumeChange_None)
                            {
                                if (i >= 1 && currrVolChange != volumeChange_seg)
                                    std::cout << "Inconsistent Volume Change among different tracks." << std::endl;
                                volumeChange_seg = currrVolChange;
                            }

                            for (Ms::Note* n : chord->notes()) {
                                museprotocol::Note* noteprotobuf = currSegment->add_note();
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

                                noteprotobuf->set_duration(duration.ticks());
                                noteprotobuf->set_pitch(n->pitch()); // 0-127
                                noteprotobuf->set_vol(vol_enum);
                                // Instrument
                                fillInstrumentField(n, instr, noteprotobuf);

                            }
                        }
                    }
                    currSegment->set_volchange(volumeChange_seg);

                }

                // exclude 
                s = s->next();



            }


            m = m->nextMeasure();

        }
}

void extractTracks(Ms::Score* currscore, museprotocol::Score& score) {
    Ms::Measure* m = currscore->firstMeasure();
    Fraction currTimeSig; // same for at least one measure
    int measureIdx = 0;
    std::vector<museprotocol::Track*> track_ptrs;
    
    while (m) {
        measureIdx++;
        Ms::Segment* s = m->first(SegmentType::TimeSig);
        if (s) {
            // Time Signature, not necessarily used. Record at most one signature here
            TimeSig* timesig = static_cast<TimeSig*>(s->element(0));
            currTimeSig = timesig->sig();
        }

        // ChordRest segments
        s = m->first(SegmentType::All);
        const std::set<SegmentType> exclude = {};
        while (s) {
            // only ChordRest?
            if (s->segmentType() == SegmentType::ChordRest) {
                const std::vector<Element*>& elements = s->elist();
                // initialize
                if (track_ptrs.empty()) {
                    for (int i = 0; i < elements.size() / VOICES; i++)
                        track_ptrs.push_back(score.add_tracks());
                }
                // add notes
                for (int i = 0; i < elements.size(); i++) {
                    int trackIdx = i / VOICES;
                    auto e = elements[i];
                    if (e && e->isChord()) {
                        Ms::Chord* chord = static_cast<Ms::Chord*>(e);
                        
                    }
                }
            }
            s = s->next();
        }
        m = m->nextMeasure();
    }
}

int main(int argc, char* argv[]) {
    // parse args (3 args)
    if (argc < 3) {
        std::cerr << "Too few arguments provided." << std::endl;
        return 1;
    }
    if (argc >= 4)
        std::cout << "Warning: too many arguments." << std::endl;
    std::string score_dir_path = argv[1];
    std::string output_path = argv[2];
    if (output_path.back() == '/' || output_path.back() == '\\')
        output_path.pop_back();
    if (output_path.empty())
        output_path.push_back('.');
    if (!fs::exists(output_path))
        fs::create_directories(output_path);
    std::string midi_path = output_path+"/midi";
    if (!fs::exists(midi_path))
        fs::create_directories(midi_path);

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
    
    constexpr int buff_len = 1024 * 1024 * 50;
    std::vector<char> buffer(buff_len);

    for (const auto& p : fs::directory_iterator(score_dir_path)) {
        std::string score_path = p.path().generic_string();
        std::shared_ptr<MasterScore> currscore = std::make_shared<MasterScore>(mscoreGlobal.baseStyle());
        mu::notation::MsczNotationReader reader;
        reader.read(currscore.get(), score_path);
        currscore->updateVelo();
        std::string filenameBase = filenamefromString(score_path);

        // output midi
        ExportMidi midif(currscore.get());
        
        midif.write(QString::fromStdString(midi_path + '/' + filenameBase + ".mid"), true, true);

        museprotocol::Score score;


        
        int bytesize = score.ByteSizeLong();
        score.SerializeToArray(buffer.data(), bytesize);
        std::ofstream f(output_path + '/' + filenameBase + ".bin", std::ios::binary);
        f.write(buffer.data(), bytesize);
    }

    
    



    return 0;
}