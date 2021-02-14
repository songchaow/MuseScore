#pragma once
#include <vector>
#include <string>
#include <set>
#include <map>

// unfinished
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