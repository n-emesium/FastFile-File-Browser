#include <iostream>
#include <vector>
#include <filesystem>
#include <chrono>
#include <thread>
#include <fstream>
#include "libraries/json.hpp"
#include <regex>
#include <mutex>

using namespace std;
namespace fs = std::filesystem;
using json = nlohmann::json;

void helper(const vector<fs::path> &dirs);
void indexer(const fs::directory_entry &ent);
bool validator(const string &string);

std::mutex data_mutex;
std::mutex count_mutex;
const int MAX_COUNT = 20;
const int MAX_THREADS = 500;
int COUNT = 0;
json fileData;
json extData;
regex rep(" ");

int main()
{
    ofstream jsonFile;
    ofstream extFile;

    jsonFile.open("fileIndex.json");
    extFile.open("extIndex.json");

    try
    {

        fs::path root_path = R"(C:\)";
        vector<fs::path> initial_dirs;

        // Get the initial set of directories at the root level
        for (const auto &entry : fs::directory_iterator(root_path))
        {
            indexer(entry);
            if (fs::is_directory(entry.status()))
            {
                initial_dirs.push_back(entry.path());
            }
        }
        // Split directories among threads
        vector<vector<fs::path>> thread_dirs(initial_dirs.size());
        for (size_t i = 0; i < initial_dirs.size(); ++i)
        {
            thread_dirs[i % MAX_THREADS].push_back(initial_dirs[i]);
        }

        vector<thread> threads;
        chrono::steady_clock::time_point begin = chrono::steady_clock::now();

        // Launch threads
        for (int i = 0; i < initial_dirs.size(); ++i)
        {
            threads.emplace_back(helper, ref(thread_dirs[i]));
        }

        // Wait for threads to finish
        int total_count = 0;
        for (auto &t : threads)
        {
            total_count++;
            t.join();
        }

        chrono::steady_clock::time_point end = chrono::steady_clock::now();
        cout << "Total count: " << total_count << endl
             << "File Count: " << COUNT << endl;
        cout << "Elapsed time: " << chrono::duration_cast<chrono::microseconds>(end - begin).count() / 1000000.0 << " seconds" << endl;
        jsonFile << fileData.dump(1);
        extFile << extData.dump(1);
        jsonFile.close();
        extFile.close();
    }
    catch (const fs::filesystem_error &e)
    {
        cerr << "Filesystem error: " << e.what() << endl;
    }
    catch (const std::exception &e)
    {
        cerr << "General error: " << e.what() << endl;
    }
}

void helper(const vector<fs::path> &dirs)
{
    for (const auto &dir : dirs)
    {
        vector<fs::path> stack;
        stack.push_back(dir);

        while (!stack.empty())
        {
            fs::path current_dir = stack.back();
            stack.pop_back();

            try
            {
                for (const auto &entry : fs::directory_iterator(current_dir))
                {
                    cout << entry.path().string() << endl;
                    indexer(entry);

                    if (fs::is_directory(entry.status()))
                    {
                        {
                            std::lock_guard<std::mutex> guard(count_mutex);
                            if (COUNT >= MAX_COUNT)
                            {
                                return;
                            }
                            COUNT++;
                        }
                        stack.push_back(entry.path());
                    }
                }
            }
            catch (const fs::filesystem_error &e)
            {
                continue;
            }
        }
    }
}

void indexer(const fs::directory_entry &ent)
{
    const string &path = ent.path().string();
    const int ix = path.find_last_of('\\');
    string fileName = path.substr(ix + 1);
    json *loc_data;

    if (!fs::is_directory(ent.status()))
    {
        fileName = fileName.substr(fileName.find_last_of('.') + 1);
        std::cout << "Extracted extension: " << fileName << std::endl;

        fileName = regex_replace(fileName, rep, "");
        std::cout << "File name after regex replace: " << fileName << std::endl;

        loc_data = &extData;
    }
    else
    {
        fileName = fileName.substr(0, fileName.find_last_of('.'));
        fileName = regex_replace(fileName, rep, "");
        loc_data = &fileData; // Use a pointer to json to modify DATA directly
    }

    if (fileName.empty())
    {
        std::cerr << "Warning: Empty file name after processing for path: " << path << std::endl;
        return;
    }

    std::lock_guard<std::mutex> guard(data_mutex); // Lock the mutex to protect DATA
    cout << "Processing file: " << fileName << " for path: " << path << endl;

    for (int i = 0; i < fileName.size(); i++)
    {
        string chr(1, fileName[i]);
        if (!validator(chr))
        {
            return;
        }
        loc_data = &((*loc_data)[chr]); // Update loc_data to point to the nested object
        if (i == fileName.size() - 1)
        {
            (*loc_data)["END"].push_back(path); // Modify DATA directly
        }
    }
}


bool validator(const string &string)
{
    int bytes = 0;
    unsigned char byte;
    for (std::string::const_iterator it = string.begin(); it != string.end(); ++it)
    {
        byte = (unsigned char)(*it);
        if (bytes == 0)
        {
            if ((byte >> 5) == 0x6)
                bytes = 1;
            else if ((byte >> 4) == 0xE)
                bytes = 2;
            else if ((byte >> 3) == 0x1E)
                bytes = 3;
            else if (byte >= 0x80)
                return false;
        }
        else
        {
            if ((byte >> 6) != 0x2)
                return false;
            bytes--;
        }
    }
    return bytes == 0;
}
