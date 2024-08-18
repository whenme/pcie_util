#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include "pci_device.hpp"

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

