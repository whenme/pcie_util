#include <filesystem>


#include "pci_tree.hpp"
#include "pci_enumeration.hpp"

pci_tree::pci_tree()
{
    std::vector<std::filesystem::path> dir;
    std::filesystem::path dir_path = "/sys/bus/pci/devices";
	
    enumeration(dir_path, dir);

    for(auto& path : dir)
    {
        pci_device Id = pci_device(path);
        m_pciDeviceList.push_back(Id);
    }
}


pci_device* pci_tree::getPciDevice(std::string fileName)
{
    for(auto& dev : m_pciDeviceList)
    {
       if (dev.getPciName() == fileName)
	   {
            cout << "success" << endl;
            return &dev;
	   }
    }

    return nullptr;
}

