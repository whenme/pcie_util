#include <filesystem>
#include <algorithm>


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


std::string print_pci_tree()
{
    std::vector<std::string> aTree;
    std::vector<pci_device> devTree;
    pci_tree tree;
    devTree = tree.getPciDeviceList();
	
    cout << "-[0000:00]-" ;
	
    for(auto& dev : devTree)
        aTree.push_back(dev.getPciName().erase(0, 8));
	
    std::sort(aTree.begin(), aTree.end());

    for(auto& i : aTree)
    {
        if(i == aTree.front())
            cout << "+-" << i << '\n';
        else if(i != aTree.front() && i !=aTree.back())
            cout << "           +-" << i << '\n';
        else
            cout << "           \\-" << i << '\n';
    }
	
}