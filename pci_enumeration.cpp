#include <iostream>
#include <fstream>
#include <string>
#include <fstream>
#include <cstring>
#define FMT_HEADER_ONLY

#include <fmt/format.h>
#include <fmt/core.h>


#include "pci_enumeration.hpp"

/*void ergodic(const path& p)
{
    if(!exists(p))
    {
        return;
    }

    recursive_directory_iterator begin { p };
    recursive_directory_iterator end { };
    for (auto iter { begin }; iter != end; ++iter)
	{
        const string spacer(iter.depth() * 1,' ');
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
*/
/*void listFiles(const string * dir)

int enumeration(const path& p)
{
    strcat(p,"*.*");
    listFiles(p);
}

void listFiles(const string * dir)
{
    intptr_t handle;
    _finddata_t findData;

    handle = _findfirst(dir,&findData);
    if(handle == -1)
    {
        cout << "Failed to find first file! "<< endl;
        return;
    }
    do
    {
        if(findData.attrib & _A_SUBDIR && strcmp(findData.name,".") == 0 && strcmp(findData.name,"..") == 0)
            cout << findData.name << "\t<dir>" << endl;
        else
            cout << findData.name << "\t" << findData.size << endl;
    }while(_findnext(handle, &findData) == 0 );
    cout << "Done!" << endl;
    _findclose(handle);
}*/
void enumeration(const string& path, std::vector<std::filesystem::path> &dir)
{
    cout << "Path: " << path << endl;
    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        dir.push_back(entry.path());
        cout << entry.path() << endl;
    }
}