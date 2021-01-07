#include "score.h"
#include "internal/masternotation.h"
#include "internal/mscznotationreader.h"
#include "libmscore/preferences.h"
#include "libmscore/musescoreCore.h"
#include "framework/fonts/fontsmodule.h"
#include <string>
#include <memory>

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
	Segment* s = currscore->firstSegment(SegmentType::All);




	return 0;
}