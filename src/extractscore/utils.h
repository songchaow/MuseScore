#pragma once
#include <vector>
#include <string>
#include <set>
#include <map>
#include <unordered_map>

// unfinished
std::map<std::string, std::string> paresArgs(int argc, char* argv[]);


std::vector<std::string> splitpath(
    const std::string& str
    , const std::set<char> delimiters);

std::string filenamefromString(const std::string& s);

struct FileStatusRecordIt {
private:
    int curr_ver;
    std::unordered_map<std::string, int>::iterator curr_it;
    std::unordered_map<std::string, int>::iterator end;
    void forward() {
        if (curr_it == end)
            return;
        ++curr_it;
        while (curr_it != end && curr_it->second == curr_ver)
            ++curr_it;
    }
public:
    FileStatusRecordIt& operator++() {
        forward();
        return *this;
    }
    FileStatusRecordIt operator++(int) {
        FileStatusRecordIt proxy(curr_it, end, curr_ver);
        forward();
        return proxy;
    }
    bool operator!=(const FileStatusRecordIt& b) { return curr_it != b.curr_it; }
    std::pair<const std::string, int>& operator*() { return *curr_it; }
    FileStatusRecordIt(std::unordered_map<std::string, int>::iterator begin_it,
        std::unordered_map<std::string, int>::iterator end, int curr_ver) :
        curr_it(begin_it), end(end), curr_ver(curr_ver) {}

};

struct FileStatusRecord {
private:
    std::string dir;
    int curr_ver;
    std::unordered_map<std::string, int> version_map;
    std::unordered_map<std::string, int>::iterator begin_it;
    void buildVersionMap(std::string dir);
    void writeVersionMap();
    void readVersionMap();
public:
    static constexpr auto record_name = "meta.txt";
    FileStatusRecord(std::string dir);
    ~FileStatusRecord();
    int currVersion() { return curr_ver; }
    void newVersion() { curr_ver++; begin_it = version_map.begin();}
    FileStatusRecordIt begin() { return  FileStatusRecordIt(begin_it, version_map.end(), curr_ver); }
    FileStatusRecordIt end() { return  FileStatusRecordIt(version_map.end(), version_map.end(), curr_ver); }
};