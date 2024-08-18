#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <vector>

#include "pci_device.hpp"
#include "pci_enumeration.hpp"
#include "cmdline.hpp"
#include "pci_tree.hpp"

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

    
    pci_device* pciDevice;
	
    std::string fileName;
    cin >> fileName;
    pciDevice = tree.getPciDevice(fileName);
    if(pciDevice == nullptr)
        cout << "failed" << endl;

    return 0;
}