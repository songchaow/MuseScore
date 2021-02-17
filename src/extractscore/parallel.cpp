#include "utils.h"
#include <iostream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <cstdlib>
#include "stdio.h"
namespace fs = std::filesystem;

std::mutex record_lock;

bool occupied[10000] = {false};

std::string score_dir_path;
std::string output_path;
const std::string program_name = "single";

#if 0
std::string exec(const char* cmd) {
    char buffer[128];
    std::string result = "";
    FILE* pipe = popen(cmd, "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    try {
        while (fgets(buffer, sizeof buffer, pipe) != NULL) {
            result += buffer;
        }
    }
    catch (...) {
        pclose(pipe);
        throw;
    }
    pclose(pipe);
    return result;
}
#endif

// executed by each thread
void runJob(FileStatusRecord* record) {

    // std::vector<std::pair<std::string, int>> local_jobs;
    // {
    //     // collect jobs
    //     int unfinishedIdx = 0;
    //     int currVersion = record->currVersion();
    //     const std::lock_guard<std::mutex> lock(record_lock);
    //     for(auto &i : *record) {
    //         if(i.second == currVersion)
    //             continue;
    //         if(unfinishedIdx % totalThread == threadIdx) {
    //             local_jobs.push_back(i);
    //         }
    //         unfinishedIdx++;
    //     }

    // }
    // run loop
    auto it = record->begin();
    int idx = 0;
    std::pair<std::string, int> chosen;
    for(;;) {
        // collect one job
        {
            const std::lock_guard<std::mutex> lock(record_lock);
            // find one unfinished and unoccupied
            for(;it != record->end();it++,idx++) {
                if((*it).second == record->currVersion())
                    continue;
                if(occupied[idx])
                    continue;
                chosen = *it;
                occupied[idx] = true;
                break;
            }
            if(it==record->end())
                break;
        }
        // execute
        std::string score_path = score_dir_path+'/'+chosen.first;
        std::string cmd = "";
        cmd += program_name + ' ';
        cmd += '\"' + score_path + '\"' + ' ';
        cmd += '\"' + output_path + '\"';
        int retval = std::system(cmd.c_str());
        // save in memory and IN FILE
        {
            const std::lock_guard<std::mutex> lock(record_lock);
            occupied[idx] = false;
            if(retval==0)
                (*it).second = record->currVersion();
            record->writeVersionMap();
        }
    }
}

/*
    Args:
        score_dir: str
        output_dir: str
        //parallel count: int
*/
int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Too few arguments provided." << std::endl;
        return 1;
    }
    if (argc >= 4)
        std::cout << "Warning: too many arguments." << std::endl;
    score_dir_path = argv[1];
    output_path = argv[2];
    //const int parallel_count = std::stoi(argv[3]);
    if (output_path.back() == '/' || output_path.back() == '\\')
        output_path.pop_back();
    if (output_path.empty())
        output_path.push_back('.');
    if (!fs::exists(output_path))
        fs::create_directories(output_path);
    std::string midi_path = output_path+"/midi";
    if (!fs::exists(midi_path))
        fs::create_directories(midi_path);

    FileStatusRecord binrecord(score_dir_path);
    std::vector<std::thread> thread_pool;
    std::cout << "Multithread begin" << std::endl;
    int thread_count = std::thread::hardware_concurrency();
    thread_count = 2;
    for(int i = 0; i < thread_count; i++) {
        thread_pool.push_back(std::thread(runJob, &binrecord));
    }

    for(int i = 0; i < thread_count; i++)
        thread_pool[i].join();


}