#include <bitset>
#include <vector>

#include "pci_tree.hpp"
#include "pci_region.hpp"
#include "pci_device.hpp"

int read_region(std::string& fileName, int& region_amount)
{
    pci_tree pci;
    pci_device* pciDevice;
    pciDevice = pci.getPciDevice(fileName);
	
    if(pciDevice == nullptr)
        std::cout << "failed" << std::endl;
    else
    {
	    int i = 1;
		
        std::string config = pciDevice->getPciConfig();
        std::cout << "Path: " << fileName << " : " << std::endl;
        for (char c : config) 
        {
            std::cout << std::bitset<8>(static_cast<int>(static_cast<unsigned char>(c))) << " ";
			i++;
		    if(i == 9)
            {
                std::cout << '\n';
                i = 1;
            }
        }
		
		
        std::cout << '\n';
        i = 1;
		int r_a = 0;
        for(char r : config)
        {
            i++;
            if(i >= 18 && i <= 17 + region_amount * 4)
                std::cout << std::bitset<8>(static_cast<int>(static_cast<unsigned char>(r))) << " ";
            if((i - 17) % 4 == 0 && i <= 16 + region_amount * 4 && i >= 18)
            {
                std::cout << '\n';
            }
            if((i - 17) % 4 == 0 && i <= 16 + region_amount * 4 && i >= 17)
            {
                std::cout << "region" << r_a << ": " ;
                r_a += 1;
            }
        }
		std::cout << '\n';
    }
	return 0;
}