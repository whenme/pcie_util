#include <iostream>
#include <fstream>
#include <string>

#define FMT_HEADER_ONLY

#include <fmt/format.h>
#include <fmt/core.h>


#include "read_file.hpp"

using namespace std::filesystem;

void ergodic(const path& p)
{
    if(!exists(p))
    {
        return;
    }
    recursive_directory_iterator begin { p };
    recursive_directory_iterator end { };
    for (auto iter { begin }; iter != end; ++iter)
	{
        const string spacer(iter.depth() * 2,' ');
        auto& entry { *iter };
        if(is_regular_file(entry))
	    {
            cout << fmt::format("{}File: {} ({} bytes)",
                spacer, entry.path().string(), file_size(entry)) << endl;
        }
        else if(is_directory(entry))
        {
            cout << fmt::format("{}Directory: {}", spacer, entry.path().string()) << endl;
        }
    }
}

void read_file()
{
    /*using namespace std;
	
	ofstream fout(filename.c_str());
	
	fout*/
	
	path p1{ "/sys/bus/pci" };
	ergodic(p1);
}