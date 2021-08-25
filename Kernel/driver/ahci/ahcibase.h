#pragma once
#include <driver/bus/pcie/pcie.h>
#include <driver/iface/blockdevice.h>
#include <mem/vas.h>
#include <util/timer.h>
#include <util/types.h>

class AHCIBlockDevice;

/**
 * @brief Class for managing singular and all AHCI devices in the system
 */
class AHCI {

public:

    /**
     * @brief Constructor of AHCI device
     * @param device PCI device which was identified and is known to be AHCI compatible AHCI controller
     */
    AHCI(PCIDevice *device);

    /**
     * @brief Gives information if AHCI controller was successfully initialized
     * @return true if initialization was successfull, false otherwise
     */
    bool initializedCorrectly();

    /**
     * @brief Gives information about sector count of device attached at specific port of AHCI device
     * @param port Number of port (0-31)
     * @return Sector count of sepcific device
     */
    usz getSectorCount(u8 port);

    /**
     * @brief Reads sectors from specific device
     * @param port Number of port (0-31)
     * @param sectorStart Starting sector of media access
     * @param sectorCount Count of sectors to be read
     * @param buffer Buffer, to which the data will be trasferred
     * @param handler Callback which is called after reading
     * @param handlerData Data to be passed to callback function
     * @return true if request was successfully sent to device, false otherwise
     */
    bool readSectors(u8 port, usz sectorStart, usz sectorCount, VirtualMemoryObject *buffer, EventHandler handler, void *handlerData);

    /**
     * @brief Initialize all AHCI devices found in a system
     */
    static void initialize();

    /**
     * @brief Returns list of all SATA block devices found in the system in operational state
     * @return List of IBlockDevice objects representing attached SATA block devices
     */
    static List<IBlockDevice*> *getBlockDevices();

private:

    struct PortControl {
        u32 commandListBaseAddress;
        u32 commandListBaseAddressUpper;
        u32 fisBaseAddress;
        u32 fisBaseAddressUpper;
        u32 interruptStatus;
        u32 interruptEnable;
        u32 commandAndStatus;
        u32 reserved1;
        u32 taskFileData;
        u32 signature;
        u32 sataStatus;
        u32 sataControl;
        u32 sataError;
        u32 sataActive;
        u32 commandIssue;
        u32 sataNotification;
        u32 fisBasedSwitchingControl;
        u32 deviceSleep;
        u32 reserved2[10];
        u32 vendorSpecific[4];
    };

    struct ABAR {
        u32 hostCapabilities;
        u32 globalHostControl;
        u32 interruptStatus;
        u32 portsImplemented;
        u32 version;
        u32 commandCompletionCoalescingControl;
        u32 commandCompletionCoalescingPorts;
        u32 enclosureManagementLocation;
        u32 enclosureManagementControl;
        u32 hostCapabilitiesExtended;
        u32 biosHandoff;
        u32 reserved[13];
        u32 reservedForNVMHCI[16];
        u32 vendorSpecific[24];
        volatile PortControl ports[32];
    };

    struct CommandHeader {
        u8 commandFISLength : 5;
        u8 atapi : 1;
        u8 write : 1;
        u8 prefetchable : 1;

        u8 reset : 1;
        u8 bist : 1;
        u8 clearBusy : 1;
        u8 reserved1 : 1;
        u8 portMultiplierPort : 4;

        u16 prdtLength;

        volatile u32 byteCount;

        u32 commandTableBaseAddress;
        u32 commandTableBaseAddressUpper;
        
        u32 reserved2[4];
    } __attribute__((packed));

    struct PhysicalRegionDescriptor {
        u32 dataBaseAddress;
        u32 dataBaseAddressUpper;
        u32 reserved1;
        u32 byteCount : 22;
        u32 reserved2 : 9;
        u32 interrupt : 1;
    } __attribute__((packed));

    struct CommandTable {
        u8 commandFIS[64];
        u8 atapiCommand[16];
        u8 reserved[48];
        PhysicalRegionDescriptor physicalRegionDescriptorTable[1];
    } __attribute__((packed));

    struct Request {
        usz sector;
        usz count;
        bool write;
        EventHandler handler;
        void *handlerData;
    };

    struct PortInfo {
        u8 portNumber;
        UncacheablePageVirtualMemoryObject *receivedFISObject;
        void *receivedFISAddress;
        UncacheablePageVirtualMemoryObject *commandListObject;
        volatile CommandHeader *commandList;
        UncacheablePageVirtualMemoryObject *commandTableObjects[32];
        volatile CommandTable *mappedCommandTables[32];
        MemoryBackedVirtualMemoryObject *identifyObject;
        void *mappedIdentifyData;
        
        Spinlock portSpinlock;
        bool identified = false;
        u32 commandsInUse = 0;
        Request currentRequests[32];
        List<Request> queuedReads;
        usz sectorSize;
        usz sectorCount;
    };

    struct CommandFIS {
        u8 fisType;
        u8 portMultiplierPort : 4;
        u8 reserved1 : 3;
        u8 isCommand : 1;
        u8 command;
        u8 featureLow;
        u8 lba0;
        u8 lba1;
        u8 lba2;
        u8 device;
        u8 lba3;
        u8 lba4;
        u8 lba5;
        u8 featureHigh;
        u8 countLow;
        u8 countHigh;
        u8 isochronousCommandCompletion;
        u8 control;
        u32 reserved2;
    } __attribute__((packed));

    static constexpr u8 ataCommandIdentify = 0xec;
    static constexpr u8 ataCommandReadDMAEx = 0x25;

    bool initializePort(u8 portNumber);
    void identifyDevices();
    bool issueCommand(u8 port, u8 command, u16 transferSectors, usz accessSector, bool mediaAccess, bool write, VirtualMemoryObject *data, EventHandler handler, void *handlerData);
    void handleInterrupt();

    static void interruptHandler(void *data, u32);

    static inline List<AHCI*> *devices = nullptr;
    static inline List<IBlockDevice*> *blockDevices = nullptr; 

    PCIDevice *pciDevice;
    MMIOVirtualMemoryObject *vmObject = nullptr;
    ABAR volatile *abar = nullptr;
    PortInfo *portInformation = nullptr;
    List<PortInfo*> *drives = nullptr;

    u8 numberOfPorts = 0;
    u8 numberOfCommandSlots = 0;
    u32 ahciVersion = 0;
    bool supports64Bit = false;
    bool initialized = false;
    bool staggeredSpinUp = false;

};

class AHCIBlockDevice : public IBlockDevice {

public:

    AHCIBlockDevice(AHCI *underlyingAHCI, u8 portNumber);

    bool isWriteable() override;
    usz sectorCount() override;
    bool read(usz sector, usz count, VirtualMemoryObject *buffer, EventHandler handler, void *handlerData) override;
    bool write(usz sector, usz count, VirtualMemoryObject *buffer, EventHandler handler, void *handlerData) override;

private:
    AHCI *ahci;
    u8 port;

};