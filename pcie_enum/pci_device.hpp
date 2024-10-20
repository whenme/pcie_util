#ifndef pci_device_H_
#define pci_device_H_

#include <string>
#include <filesystem>


class pci_device 
{
public:
    pci_device(std::filesystem::path filePath);
    ~pci_device() = default;

    std::string getFilePath()  { return m_filePath;     }
    uint32_t getDeviceId()     { return m_deviceId;     }
    uint32_t getVendorId()     { return m_vendorId;     }
    uint32_t getClassId()      { return m_classId;      }
    std::string getPciConfig() { return m_config;       }
    std::string getPciName()   { return m_pciName;      }
    uint32_t getRegionAmount() { return m_regionAmount; }

private:
    uint32_t getFileContext(std::string fileName);
    std::string getFileContext2();
    std::string getFileContext3(std::string fileName);

    std::string m_filePath;
    uint32_t    m_deviceId;
    uint32_t    m_vendorId;
    uint32_t    m_classId;
    std::string m_config;
    std::string m_pciName;
    uint32_t    m_regionAmount;

};


#endif