#ifndef __PCI_TREE_H__
#define __PCI_TREE_H__

#include <vector>
#include <string>
#include <iostream>
#include <iterator>

#include "pci_device.hpp"

class pci_tree
{
public:
    pci_tree();
    ~pci_tree() = default;

    std::vector<pci_device>& getPciDeviceList() { return m_pciDeviceList; }
    pci_device* getPciDevice(std::string fileName); 	


private:
    std::vector<pci_device> m_pciDeviceList;
    std::vector<std::string> m_pciDevice;
};

#endif


std::string print_pci_tree();