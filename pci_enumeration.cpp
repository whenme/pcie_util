#include <iostream>
#include <string>


#include "pci_enumeration.hpp"

void enumeration(const string& path, std::vector<std::filesystem::path> &dir)
{
    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        dir.push_back(entry.path());
    }
}