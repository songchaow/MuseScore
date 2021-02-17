#include "utils.h"
#include <filesystem>
#include <fstream>
namespace fs = std::filesystem;

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


std::vector<std::string> splitpath(
    const std::string& str
    , const std::set<char> delimiters)
{
    std::vector<std::string> result;

    char const* pch = str.c_str();
    char const* start = pch;
    for (; *pch; ++pch)
    {
        if (delimiters.find(*pch) != delimiters.end())
        {
            if (start != pch)
            {
                std::string str(start, pch);
                result.push_back(str);
            }
            else
            {
                result.push_back("");
            }
            start = pch + 1;
        }
    }
    result.push_back(start);

    return result;
}

std::string filenamefromString(const std::string& s) {
    static std::set<char> ds = { '\\','/' };
    std::vector<std::string> parsed = splitpath(s, ds);
    if (s.size() == 0)
        return std::string();
    return parsed.back();
}

void FileStatusRecord::buildVersionMap(std::string dir) {
    version_map.clear();
    for(auto file : fs::directory_iterator(dir)) {
        auto s = file.path().filename().generic_string();
        version_map.push_back({ s,0 });
    }
}

void FileStatusRecord::readVersionMap() {
    std::string record_path = dir + '\\' + record_name;
    std::ifstream metaFile(record_path);
    metaFile >> curr_ver;
    for(;;) {
        std::string filename;
        int version;
        metaFile >> filename;
        metaFile >> version;
        if (metaFile)
            version_map.push_back({ filename,version });
        else
            break;
    }
}

void FileStatusRecord::writeVersionMap() {
    std::string record_path = dir + '\\' + record_name;
    std::ofstream metaFile(record_path);
    metaFile << curr_ver << std::endl;
    for (auto i : version_map) {
        metaFile << i.first << ' ' << i.second << std::endl;
    }
}

FileStatusRecord::FileStatusRecord(std::string dir) : dir(dir) {
    std::string record_path = dir + '\\' + record_name;
    if (fs::exists(record_path)) {
        readVersionMap();
        // if stored curr_ver >0, that's it. if ==0, we make curr_ver 1
        if(curr_ver <= 0)
            curr_ver = 1;
    }
    else {
        // initialize all entries with version 0
        buildVersionMap(dir);
        curr_ver = 1;
    }
    // init begin_it
    begin_it = version_map.begin();
    while(begin_it != version_map.end() && begin_it->second==curr_ver)
        begin_it++;
    
}

FileStatusRecord::~FileStatusRecord() {
    writeVersionMap();
}