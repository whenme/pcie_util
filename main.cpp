#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <vector>

#include "pci_device.hpp"
#include "pci_enumeration.hpp"
#include "cmdline.hpp"
#include "pci_tree.hpp"
#include "region_amount.hpp"
#include "pci_region.hpp"

int32_t main(int argc, char *argv[])
{

    std::vector<pci_device> devTree;

    pci_tree tree;

    devTree = tree.getPciDeviceList();

    cmdline::parser a;

    a.add("device", 'd', "device Id");
    a.add("vendor", 'v', "vendor Id");	
    a.add("class" , 'c', "class  Id");
    a.add("tree",   't', "pci tree" );
    a.add<std::string>("id", 'i', "id of pci device", false, "");

    a.parse_check(argc, argv);


    if (a.exist("device")) 
    {
        for (auto& dev : devTree)
            cout << "Path:" << dev.getFilePath() << ": device id: " << dev.getDeviceId() << endl;
    }
	
    if (a.exist("vendor")) 
    {
        for (auto& dev : devTree)
            cout << "Path:" << dev.getFilePath() << ": vendor id: " << dev.getVendorId() << endl;
    }
	
    if (a.exist("class")) 
    {
        for (auto& dev : devTree)
            cout << "Path:" << dev.getFilePath() << ": class id: "  << dev.getClassId()  << endl;
    }

    if(a.exist("id"))
    {
        pci_device* pciDevice;
	
        std::string fileName;
        fileName = a.get<std::string>("id");
        pciDevice = tree.getPciDevice(fileName);
        if(pciDevice == nullptr)
            cout << "failed" << endl;
        else
        {
            int regionAmount = getPciDeviceRegionAmount(fileName);
            if(regionAmount >= 0)
            {
                cout << "The PCI device " << fileName << " has " << regionAmount << " regions." << endl;
            }
    
            int region_amount = regionAmount;
            read_region(fileName, region_amount);
        }
    }		
    /*pci_device* pciDevice;
	
    std::string fileName;
    cin >> fileName;
    pciDevice = tree.getPciDevice(fileName);
    if(pciDevice == nullptr)
        cout << "failed" << endl;
    else
    {
        int regionAmount = getPciDeviceRegionAmount(fileName);
        if(regionAmount >= 0)
        {
            cout << "The PCI device " << fileName << " has " << regionAmount << " regions." << endl;
        }

        int region_amount = regionAmount;
        read_region(fileName, region_amount);
    }*/
	
    return 0;
}