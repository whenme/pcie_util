#ifndef pci_device_H_
#define pci_device_H_

#include <string>
#include <filesystem>


class pci_device 
{
public:
    pci_device(std::filesystem::path filePath);
    ~pci_device() = default;

    std::string getFilePath(){ return m_filePath; }
    uint32_t getDeviceId()   { return m_deviceId; }
    uint32_t getVendorId()   { return m_vendorId; }
    uint32_t getClassId()    { return m_classId;  }
    std::string getPciName() { return m_pciName;  }

private:
    uint32_t getFileContext(std::string filePath);
    std::string getFileContext2();

    std::string m_filePath;
    uint32_t    m_deviceId;
    uint32_t    m_vendorId;
    uint32_t    m_classId;
    std::string m_pciName;

};


#endif