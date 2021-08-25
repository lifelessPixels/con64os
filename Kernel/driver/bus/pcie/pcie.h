#pragma once
#include <util/types.h>
#include <util/list.h>
#include <driver/acpi/acpibase.h>
#include <driver/arch/ints.h>
#include <mem/vas.h>

class PCIDevice;

/**
 * @brief Class for managing specific PCIe Bus Segment
 */
struct PCIeBusSegment {

    /**
     * @brief Reads 32-bit value from configuration space of specified device
     * @param bus Bus value
     * @param device Device value
     * @param function Function value
     * @param offset Offset in configuration space
     * @return Read value
     */
    u32 read(u8 bus, u8 device, u8 function, u16 offset);

    /**
     * @brief Writes 32-bit value to configuration space of specified device
     * @param bus Bus value
     * @param device Device value
     * @param function Function value
     * @param offset Offset in configuration space
     * @param value Value to be written
     */
    void write(u8 bus, u8 device, u8 function, u16 offset, u32 value);

    /**
     * @brief Physical address of PCIe Bus Segment
     */
    u64 physicalAddress;

    /**
     * @brief Virtual mapped address of PCIe Bus Segment
     */
    void *virtualAddress;

    /**
     * @brief Group number of PCIe Bus Segment
     */
    u16 groupNumber;

    /**
     * @brief Decoded bus number lower bound
     */
    u8 pciBusNumberStart;

    /**
     * @brief Decoded bus number upper bound
     */
    u8 pciBusNumberEnd;
    
};

/**
 * @brief Class for managing PCIe subsystem
 */
class PCIe {

public:
    
    /**
     * @brief Initializes PCIe subsystem
     */
    static void initialize();

    /**
     * @brief Returns list of matching devices connected to the system
     * @param list List where devices should be put
     * @param classCode Class code of searched devices
     * @param subclassCode Subclass Code of searched devices
     * @param interface Interface value of searched devices
     */
    static void getDevicesByClassCodes(List<PCIDevice*> *list, u8 classCode, u8 subclassCode, u8 interface);

private:
    
    struct PCIeSegmentDescriptor {
        u64 address;
        u16 groupNumber;
        u8 pciBusNumberStart;
        u8 pciBusNumberEnd;
        u32 reserved;
    } __attribute__((packed));

    struct MCFG : public ACPI::Table {
        u64 reserved;
        PCIeSegmentDescriptor decriptors[1];
    } __attribute__((packed));

    static void enumerateDevices();

    static inline MCFG *mcfgTable = nullptr;
    static inline List<PCIeBusSegment*> *segments = nullptr;
    static inline List<PCIDevice*> *devices = nullptr;

};

/**
 * @brief Class encapsulating single PCI device
 */
class PCIDevice {

public:

    /**
     * @brief Construtor
     * @param busSegment Bus segment of created device
     * @param bus Bus to which the device is attached
     * @param device Device number on the bus
     * @param function Function number
     */
    PCIDevice(PCIeBusSegment *busSegment, u8 bus, u8 device, u8 function);

    /**
     * @brief Returns vendor ID of the device
     * @return Vendor ID of the device
     */
    u16 getVendorID();

    /**
     * @brief Returns device ID of the device
     * @return Device ID of the device
     */
    u16 getDeviceID();

    /**
     * @brief Returns class code of the device
     * @return Class code of the device
     */
    u8 getClassCode();

    /**
     * @brief Returns subclass of the device
     * @return Subclass of the device
     */
    u8 getSubclassCode();

    /**
     * @brief Returns programming interface of the device
     * @return Programming interface of the device
     */
    u8 getProgrammingInterface();

    /**
     * @brief Returns revision ID of the device
     * @return Revision ID of the device
     */
    u8 getRevisionID();

    /**
     * @brief Returns header type of the device
     * @return Header type of the device
     */
    u8 getHeaderType();

    /**
     * @brief Returns BAR value of the device
     * @param barNumber Number of the BAR in question (0-5)
     * @return BAR value
     */
    u32 getBARValue(u8 barNumber);

    /**
     * @brief Returns whether device supports MSI operation
     * @return true if MSI is supported by the device, false otherwise
     */
    bool supportsMSI();

    /**
     * @brief Enables MSI operation with selected interrupt vector
     * @param vector Vector of interrupt to be raised
     */
    void enableMSI(u8 vector);

    /**
     * @brief Enabled bus mastering of the device
     */
    void enableBusMastering();

    /**
     * @brief Disables legacy PIC interrupt handling
     */
    void disablePICInterrupts();

    /**
     * @brief Debugging funtion to dump all capabilities supported by the device
     */
    void dumpCapabilities();

private:

    struct Capability {
        u8 address;
        u8 type;
    };

    static constexpr u16 identificationOffset = 0x00;
    static constexpr u16 statusAndCommandOffset = 0x04;
    static constexpr u16 classCodesOffset = 0x08;
    static constexpr u16 miscelaneousOffset = 0x0c;
    static constexpr u16 barOffset = 0x10;
    static constexpr u16 capabilityOffset = 0x34;

    static constexpr u8 msiCapabilityID = 0x05;

    static const char *getCapabilityReadableString(u8 capabilityID);

    u32 read(u16 offset);
    void write(u16 offset, u32 value);

    PCIeBusSegment *segment;
    u8 busNumber;
    u8 deviceNumber;
    u8 functionNumber;

    u16 vendorID;
    u16 deviceID;
    u8 classCode;
    u8 subclassCode;
    u8 programmingInterface;
    u8 revisionID;
    u8 headerType;

    List<Capability> *capabilities = nullptr;
    bool msiSupported = false;
    Capability msiCapability;

};