#ifndef pci_device_H_
#define pci_device_H_

#include <string>

class pci_device 
{
public:
    pci_device(std::filesystem::path filePath);
    ~pci_device() = default;

    uint32_t getDeviceId() { return m_deviceId; }
    uint32_t getVendorId() { return m_vendorId; }
    uint32_t getClassId()  { return m_classId; }

private:
    uint32_t getFileContext(std::string filePath);

    std::string m_filePath;
    uint32_t    m_deviceId;
    uint32_t    m_vendorId;
    uint32_t    m_classId;
};

#endif