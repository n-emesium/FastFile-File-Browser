#include <iostream>
#include <vector>
#include <filesystem>
#include <chrono>
#include <thread>
#include <folly/folly/AtomicHashArray-inl.h>

using namespace std;
namespace fs = std::filesystem;

void helper(const vector<fs::path> &dirs);

const int MAX_COUNT = 100000;
const int MAX_THREADS = 500;
int COUNT = 0;
folly::StringPiece filePath = "fileIndex.json";
std::string fileContent;

int main()
{ 
    // Read the content of the JSON file into fileContent

    // Parse the file content as JSON
    folly::dynamic data = folly::parseJson(fileContent);

    // Modify the parsed JSON
    data["HI"] = folly::dynamic::object;

    // Print the modified JSON
    std::cout << folly::toJson(data) << std::endl;

    try
    {
        fs::path root_path = "C:\\Program Files git clone https://github.com/microsoft/vcpkg.git(x86)";
        vector<fs::path> initial_dirs;

        // Get the initial set of directories at the root level
        for (const auto &entry : fs::directory_iterator(root_path))
        {
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
        cout << "Total count: " << total_count << endl;
        cout << "Elapsed time: " << chrono::duration_cast<chrono::microseconds>(end - begin).count() / 1000000.0 << " seconds" << endl;
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
                    fs::path entry_path = entry.path();
                    cout << entry_path.string() << endl;

                    if (fs::is_directory(entry.status()))
                    {
                        COUNT++;
                        if (COUNT >= MAX_COUNT)
                        {
                            return;
                        }
                        stack.push_back(entry_path);
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
