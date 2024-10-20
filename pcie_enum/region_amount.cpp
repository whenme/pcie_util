#include "region_amount.hpp"



std::string aaa(std::string &content, int number)
{
    std::string temp = "";
    char t = 0;
    while(true)
    {
        t = number % 10 + '0';
        temp = t + temp;
        number /= 10;
        if(number == 0)
        {
            return (content + temp);
        }
    }
}

int getPciDeviceRegionAmount(std::string& deviceAddress)
{
    std::string regionPath = "/sys/bus/pci/devices/" + deviceAddress + "/resource";
    std::string P = regionPath;

	int regionAmount = 0;
	
    while(std::filesystem::exists(P))
    {
        P = aaa(regionPath, regionAmount);
        regionAmount++;
    }
	
    return regionAmount-1;
}
