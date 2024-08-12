#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <vector>
#include <fmt/format.h>
#define FMT_HEADER_ONLY



#include "read_file.hpp"
#include "pci_device.hpp"
#include "pci_enumeration.hpp"

int32_t main()
{
    std::vector<std::filesystem::path> dir;
    std::filesystem::path dir_path = "/sys/bus/pci/devices";
    std::vector<pci_device> devList;

    enumeration(dir_path, dir);

    for(auto& path : dir)
    {
		cout << path << endl;
	
        pci_device Id = pci_device(path);
        devList.push_back(Id);
    }

    return 0;
}