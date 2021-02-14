#include "score.h"
#include "internal/masternotation.h"
#include "internal/mscznotationreader.h"
#include "libmscore/preferences.h"
#include "libmscore/musescoreCore.h"
#include "libmscore/trill.h"
#include "libmscore/tie.h"
#include "framework/fonts/fontsmodule.h"
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

// constexpr int NUM_ARGS = 3;

int main(int argc, char* argv[]) {
#if 0
    // parse args (3 args)
    if (argc < 3) {
        std::cerr << "Too few arguments provided." << std::endl;
        return 1;
    }
    if (argc >= 4)
        std::cout << "Warning: too many arguments." << std::endl;
#endif
    std::string score_dir_path = argv[1];
    std::string output_path = argv[2];
    if (output_path.back() == '/' || output_path.back() == '\\')
        output_path.pop_back();
    if (output_path.empty())
        output_path.push_back('.');
    if (!fs::exists(output_path))
        fs::create_directories(output_path);

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

        

    }

    
    



    return 0;
}