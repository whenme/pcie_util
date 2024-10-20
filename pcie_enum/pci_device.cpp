#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>

#include "pci_device.hpp"
#include "region_amount.hpp"

using namespace std;
using namespace std::filesystem;

pci_device::pci_device(std::filesystem::path filePath)
    : m_deviceId(0),
      m_vendorId(0),
      m_classId(0)
{
    m_filePath = filePath;
    m_deviceId = getFileContext("device");
    m_vendorId = getFileContext("vendor");
    m_classId  = getFileContext("class");
    m_pciName  = getFileContext2();
    m_config   = getFileContext3("config");
    m_regionAmount = getPciDeviceRegionAmount(m_pciName);

}

uint32_t pci_device::getFileContext(string fileName)
{
    std::string filePath = m_filePath + "/" + fileName;
    fstream file;
    file.open(filePath, std::ios::in);
	
	string dId;
	getline (file, dId);
    file.close();
	
	uint32_t val = stoi (dId, nullptr, 0);
	
    return val;
}

std::string pci_device::getFileContext2()
{
    std::string path = m_filePath;
    std::string tree = path.erase(0,21);
    return tree;
}

std::string pci_device::getFileContext3(std::string fileName)
{
    std::string filePath = m_filePath + "/" + fileName;
    ifstream file(filePath, std::ios::binary);
	
    std::vector<char> data((std::istreambuf_iterator<char>(file)), (std::istreambuf_iterator<char>()));

    file.close();
	
    std::string config(data.begin(),data.end());
	
    int i = 1;
	
    return config;
}