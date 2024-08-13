#include <iostream>
#include <filesystem>
#include <chrono>

using namespace std;
namespace fs = std::filesystem;

void helper(fs::path path = "C:\\");
int COUNT = 0;
chrono::steady_clock::time_point BEGIN = std::chrono::steady_clock::now();

int main()
{
    try
    {
        helper();
    }
    catch (const fs::filesystem_error &e)
    {
        cerr << "Filesystem error: " << e.what() << endl;
    }
    catch (const std::exception &e)
    {
        cerr << "General error: " << e.what() << endl;
    }
    return 0;
}

void helper(fs::path path)
{
    fs::path p;
    try
    {
        for (const auto &entry : fs::directory_iterator(path))
        {
            p = entry.path();
            cout << p.string() << endl;
            if (fs::is_directory(entry.status()))
            {   
                COUNT++;
                if (COUNT>=2000){
                    chrono::steady_clock::time_point end = chrono::steady_clock::now();
                    cout << chrono::duration_cast<std::chrono::microseconds>(end - BEGIN).count() /1000000.0 << endl;
                    return;
                }
                helper(p);
            }
        }
    }
    catch (const fs::filesystem_error &e)
    {
        return;
    }
}
