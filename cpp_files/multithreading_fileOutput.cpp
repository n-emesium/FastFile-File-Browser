#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <regex>
#include <thread>
#include <vector>

#include "../libraries/rapidjson/document.h"
#include "../libraries/rapidjson/filereadstream.h"
#include "../libraries/rapidjson/stringbuffer.h"
#include "../libraries/rapidjson/writer.h"

using namespace std;
namespace fs = std::filesystem;
namespace rj = rapidjson;
void helper(const vector<fs::path> &dirs);
void indexer(const fs::directory_entry &ent);

std::mutex data_mutex;
std::mutex count_mutex;
const long MAX_COUNT = 50000;
const int MAX_THREADS = 4;
const int MaxSubFolderInMemory = 20000;
int COUNT = 0;

rj::Document fileData;
rj::Document extData;
rj::Document::AllocatorType &fileDataAllocator = fileData.GetAllocator();
rj::Document::AllocatorType &extDataAllocator = extData.GetAllocator();
// Convert fileData to a string and write to file
rj::StringBuffer fileDataBuffer;
rj::Writer<rj::StringBuffer> fileWriter(fileDataBuffer);  // Compact writing
// Convert extData to a string and write to file as one block
rj::StringBuffer extDataBuffer;
rj::Writer<rj::StringBuffer> extWriter(extDataBuffer);  // Compact writing
FILE *jsonFile = fopen("../fileIndex.json", "w+");
FILE *extFile = fopen("../extIndex.json", "w+");

int main() {
    fileData.SetObject();
    extData.SetObject();

    try {
        fs::path root_path = R"(C:\)";
        vector<fs::path> initial_dirs;

        // Get the initial set of directories at the root level
        for (const auto &entry : fs::directory_iterator(root_path)) {
            indexer(entry);
            if (fs::is_directory(entry.status())) {
                initial_dirs.push_back(entry.path());
            }
        }

        // Split directories among threads
        vector<vector<fs::path>> thread_dirs(initial_dirs.size());
        for (size_t i = 0; i < initial_dirs.size(); ++i) {
            thread_dirs[i % MAX_THREADS].push_back(initial_dirs[i]);
        }

        vector<thread> threads;
        chrono::steady_clock::time_point begin = chrono::steady_clock::now();

        // Launch threads
        for (int i = 0; i < initial_dirs.size(); ++i) {
            threads.emplace_back(helper, ref(thread_dirs[i]));
        }

        // Wait for threads to finish
        for (auto &t : threads) {
            t.join();
        }
        // For testing purposes
        chrono::steady_clock::time_point end = chrono::steady_clock::now();
        cout << "File Count: " << COUNT << endl;
        cout << "Elapsed time: "
             << chrono::duration_cast<chrono::microseconds>(end - begin).count() /
                    1000000.0
             << " seconds" << endl;

        fileData.Accept(fileWriter);
        fprintf(jsonFile, fileDataBuffer.GetString());

        extData.Accept(extWriter);
        fprintf(extFile, extDataBuffer.GetString());

        fclose(extFile);
        fclose(jsonFile);
    } catch (const fs::filesystem_error &e) {
        cerr << "Filesystem error: " << e.what() << endl;
    } catch (const std::exception &e) {
        cerr << "General error: " << e.what() << endl;
    }
    return 0;
}
void helper(const vector<fs::path> &dirs) {
    int fileNFolderCount = 0;
    for (const auto &dir : dirs) {
        vector<fs::path> stack;
        stack.push_back(dir);

        while (!stack.empty()) {
            fs::path current_dir = stack.back();
            stack.pop_back();

            try {
                for (const auto &entry : fs::directory_iterator(current_dir)) {
                    indexer(entry);
                    fileNFolderCount++;
                    if (fileNFolderCount == MaxSubFolderInMemory) {
                        fileNFolderCount = 0;
                    }
                    if (fs::is_directory(entry.status())) {
                        {
                            std::lock_guard<std::mutex> guard(count_mutex);
                            if (COUNT >= MAX_COUNT) {
                                return;
                            }
                            COUNT++;
                        }
                        stack.push_back(entry.path());
                    }
                }
            } catch (const fs::filesystem_error &e) {
                continue;
            }
        }
    }
}

void indexer(const fs::directory_entry &ent) {
    const string &path = ent.path().string();
    string fileName = path.substr(path.find_last_of('\\') + 1);
    rj::Document tempDoc;
    tempDoc.SetObject();
    rj::Document::AllocatorType& tempAllocator = tempDoc.GetAllocator();
    rj::Value *loc_data;
    int ix = fileName.find_last_of('.');
    std::lock_guard<std::mutex> guard(data_mutex);
    if (!fs::is_directory(ent.status()) && ix != string::npos) {
        loc_data = &tempDoc;
        for (int i = ix + 1; i < fileName.size(); i++) {
            string chr(1, fileName[i]);
            if (!isalnum(fileName[i])) {
                continue;
            }
            if (!loc_data->HasMember(chr.c_str())) {
                loc_data->AddMember(rj::Value(chr.c_str(), tempAllocator), rj::Value(rj::kObjectType), tempAllocator);
            }
            loc_data = &(*loc_data)[chr.c_str()];
            if (i == fileName.size() - 1) {
                if (!loc_data->HasMember("END")) {
                    rj::Value endArray(rj::kArrayType);
                    loc_data->AddMember("END", endArray, tempAllocator);
                }
                loc_data->FindMember("END")->value.PushBack(rj::Value().SetString(path.c_str(), tempAllocator), tempAllocator);
            }
        }
        rj::StringBuffer buffer;
        rj::Writer<rj::StringBuffer> writer(buffer);
        tempDoc.Accept(writer);
        fprintf(extFile, buffer.GetString());
    }
    fileName = fileName.substr(0, ix);
    tempDoc.SetObject();
    loc_data = &tempDoc;
    for (int i = 0; i < fileName.size(); i++) {
        if (!isalnum(fileName[i])) {
            continue;
        }
        string chr(1, tolower(fileName[i]));
        if (!loc_data->HasMember(chr.c_str())) {
            loc_data->AddMember(rj::Value(chr.c_str(), tempAllocator), rj::Value(rj::kObjectType), tempAllocator);
        }
        loc_data = &((*loc_data)[chr.c_str()]);
        if (i == fileName.size() - 1) {
            if (!loc_data->HasMember("END")) {
                rj::Value endArray(rj::kArrayType);
                loc_data->AddMember("END", endArray, tempAllocator);
            }
            loc_data->FindMember("END")->value.PushBack(rj::Value().SetString(path.c_str(), tempAllocator), tempAllocator);
        }
    }
    rj::StringBuffer buffer;
    rj::Writer<rj::StringBuffer> writer(buffer);
    tempDoc.Accept(writer);
    fprintf(jsonFile, buffer.GetString());
}
    fileName = fileName.substr(0, ix);
    loc_data = &fileData;
    for (int i = 0; i < fileName.size(); i++) {
        if (!isalnum(fileName[i])) {
            continue;
        }
        string chr(1, tolower(fileName[i]));
        if (!loc_data->HasMember(chr.c_str())) {
            loc_data->AddMember(rj::Value(chr.c_str(), fileDataAllocator), rj::Value(rj::kObjectType), fileDataAllocator);
        }
        loc_data = &((*loc_data)[chr.c_str()]);  // Update loc_data to point to the nested object
        if (i == fileName.size() - 1) {
            if (!loc_data->HasMember("END")) {
                rj::Value endArray(rj::kArrayType);
                loc_data->AddMember("END", endArray, fileDataAllocator);
            }
            loc_data->FindMember("END")->value.PushBack(rj::Value().SetString(path.c_str(), fileDataAllocator), fileDataAllocator);  // Modify DATA directly
        }
    }
}
