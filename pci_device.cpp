#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
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
    m_classId = getFileContext("class");
}

uint32_t pci_device::getFileContext(string fileName)
{
    std::string filePath = m_filePath + "/" + fileName;
    cout << "Path: " << filePath << endl;
    fstream file;
    file.open(filePath, std::ios::in);
	
	string dId;
	getline (file, dId);
    cout << "file context: " << dId <<endl;
    file.close();
	
	uint32_t val = stoi (dId, nullptr, 0);
    cout << "val = " << val << '\n' << endl;
	
    return val;
}