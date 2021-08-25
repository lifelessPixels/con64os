#include "driver/bus/pcie/pcie.h"

void PCIe::initialize() {

    // create descriptors and devices list
    segments = new List<PCIeBusSegment*>();
    devices = new List<PCIDevice*>();

    // get MCFG table
    void *mcfg = ACPI::getTableBySignature("MCFG");
    if(!mcfg) {
        Logger::printFormat("[pcie] MCFG table not found...\n");
        return;
    }

    mcfgTable = reinterpret_cast<MCFG*>(mcfg);

    // parse the table
    usz entryCount = (mcfgTable->length - sizeof(ACPI::Table) - 8) / sizeof(PCIeSegmentDescriptor);
    Logger::printFormat("[pcie] MCFG found, entry count: %u\n", entryCount);
    for(usz i = 0; i < entryCount; i++) {

        // get entry
        PCIeSegmentDescriptor *descriptor = &mcfgTable->decriptors[i];
        Logger::printFormat("[pcie]   - address: 0x%x, group number: %u, bus start: %u, bus end: %u\n", 
                            descriptor->address, descriptor->groupNumber, descriptor->pciBusNumberStart, descriptor->pciBusNumberEnd);

        // add entry to list
        PCIeBusSegment *segment = new PCIeBusSegment();
        segment->pciBusNumberStart = descriptor->pciBusNumberStart;
        segment->pciBusNumberEnd = descriptor->pciBusNumberEnd;
        segment->physicalAddress = descriptor->address;
        segment->groupNumber = descriptor->groupNumber;
        MMIOVirtualMemoryObject *object = new MMIOVirtualMemoryObject(reinterpret_cast<void*>(descriptor->address), 256ull * 1024ull * 1024ull, nullptr);
        segment->virtualAddress = VirtualAddressSpace::getKernelVirtualAddressSpace()->mapObject(object);
        if(!segment->virtualAddress) {
            Logger::printFormat("[pcie] could not map config space, aborting...\n");
            for(;;);
            // TODO: panic! 
        }
        segments->appendBack(segment);

    }

    // enumerate devices
    Logger::printFormat("[pcie] enumerating all devices...\n");
    enumerateDevices();

}

void PCIe::getDevicesByClassCodes(List<PCIDevice*> *list, u8 classCode, u8 subclassCode, u8 interface) {

    // iterate through all registered devices and get relevant ones
    for(usz i = 0; i < devices->size(); i++) {

        // get device
        PCIDevice *device = devices->get(i);

        // check device's class codes
        if(device->getClassCode() == classCode && device->getSubclassCode() == subclassCode && device->getProgrammingInterface() == interface) {
            list->appendBack(device);
        }

    }

}

void PCIe::enumerateDevices() {

    // brute force enumaration
    
    Logger::printFormat("[pcie] segments count: %u\n", segments->size());

    for(usz segment = 0; segment < segments->size(); segment++) {

        PCIeBusSegment *busSegment = segments->get(segment);

        // for every segment enumerate all buses
        for(u16 bus = busSegment->pciBusNumberStart; bus <= busSegment->pciBusNumberEnd; bus++) {

            // for every bus, enumerate all devices
            for(u8 device = 0; device < 32; device++) {

                // for every device enumerate all functions
                for(u8 function = 0; function < 8; function++) {

                    // read word and check
                    u32 identification = busSegment->read(bus, device, function, 0);
                    if((identification & 0xffff) != 0xffff) {

                        // found a device, create object of it
                        PCIDevice *foundDevice = new PCIDevice(busSegment, static_cast<u8>(bus), device, function);
                        Logger::printFormat("[pcie]   - at %u:%u:%u:%u - 0x%x:0x%x, class: %u, subclass: %u, prog if: %u (header type: %u)\n",
                                            busSegment->groupNumber, bus, device, function,
                                            foundDevice->getVendorID(), foundDevice->getDeviceID(),
                                            foundDevice->getClassCode(), foundDevice->getSubclassCode(),
                                            foundDevice->getProgrammingInterface(), foundDevice->getHeaderType());
                        
                        // dump capabilities of found device
                        foundDevice->dumpCapabilities();

                        // add newly found device to devices list
                        devices->appendBack(foundDevice);

                    }

                }

            }

        }

    }

}

u32 PCIeBusSegment::read(u8 bus, u8 device, u8 function, u16 offset) {

    // check bounds
    if(bus < pciBusNumberStart || bus > pciBusNumberEnd) return 0;
    if(device > 31) return 0;
    if(function > 7) return 0;
    if(offset >= (4096 - 4)) return 0;

    // get address
    usz address = reinterpret_cast<usz>(virtualAddress) + ((bus - pciBusNumberStart) << 20 | device << 15 | function << 12);
    u32 volatile *config = reinterpret_cast<u32*>(address);
    return config[offset / 4];

}

void PCIeBusSegment::write(u8 bus, u8 device, u8 function, u16 offset, u32 value) {

    // check bounds
    if(bus < pciBusNumberStart || bus > pciBusNumberEnd) return;
    if(device > 31) return;
    if(function > 7) return;
    if(offset >= (4096 - 4)) return;

    // get address
    usz address = reinterpret_cast<usz>(virtualAddress) + ((bus - pciBusNumberStart) << 20 | device << 15 | function << 12);
    u32 volatile *config = reinterpret_cast<u32*>(address);
    config[offset / 4] = value;

}


PCIDevice::PCIDevice(PCIeBusSegment *busSegment, u8 bus, u8 device, u8 function)
    : segment(busSegment), busNumber(bus), deviceNumber(device), functionNumber(function) {
    
    // create capability list
    capabilities = new List<Capability>();

    // get basic info about pci device in question, start with vendor and device ids
    u32 identification = read(identificationOffset);
    vendorID = identification & 0xffff;
    deviceID = identification >> 16;

    // ...then class codes and header type
    u32 classCodes = read(classCodesOffset);
    revisionID = (classCodes >> 0) & 0xff;
    programmingInterface = (classCodes >> 8) & 0xff;
    subclassCode = (classCodes >> 16) & 0xff;
    classCode = (classCodes >> 24) & 0xff;
    u32 miscelaneous = read(miscelaneousOffset);
    headerType = (miscelaneous >> 16) & 0x0f;

    // check whether it is a device and whether it has any capabilities
    u32 statusAndCommand = read(statusAndCommandOffset);
    if(headerType == 0x00 && statusAndCommand & (1 << 20)) {

        // iterate through capabilities list
        u8 capabilityPointer = read(capabilityOffset) & 0xfc;
        u32 capabilityHeader = 0;

        while(capabilityPointer != 0x00) {

            // read the header of capability
            capabilityHeader = read(capabilityPointer);

            // found a capability, add address to list
            Capability capability;
            capability.address = capabilityPointer;
            capability.type = capabilityHeader & 0xff;
            capabilities->appendBack(capability);

            // if this capability is MSI, store info about it
            if(capability.type == msiCapabilityID) {
                msiSupported = true;
                msiCapability = capability;
            }

            // go to next capability
            capabilityPointer = (capabilityHeader >> 8) & 0xfc;

        }


    }


}

u16 PCIDevice::getVendorID() { return vendorID; }
u16 PCIDevice::getDeviceID() { return deviceID; }
u8 PCIDevice::getClassCode() { return classCode; }
u8 PCIDevice::getSubclassCode() { return subclassCode; }
u8 PCIDevice::getProgrammingInterface() { return programmingInterface; }
u8 PCIDevice::getRevisionID() { return revisionID; }
u8 PCIDevice::getHeaderType() { return headerType; }

u32 PCIDevice::getBARValue(u8 barNumber) {

    // revoke accesses to non existent BARs
    if(headerType != 0x00) return 0;
    if(barNumber > 5) return 0;

    // return value
    return read(barOffset + (barNumber * 4));

}

bool PCIDevice::supportsMSI() { return msiSupported; }

void PCIDevice::enableMSI(u8 vector) {

    // if device does not support msi, just return
    if(!msiSupported) return;

    // get MSI header
    u16 msiHeader = read(msiCapability.address) >> 16;
    bool longAddress = false;
    if(msiHeader & (1 << 7)) longAddress = true;

    // set fields accordingly
    u64 msiAddress = Interrupts::getMSIAddress();
    u16 msiData = Interrupts::getMSIData(vector);
    if(longAddress) {
        // address is 64 bit, split it
        write(msiCapability.address + 0x04, msiAddress & 0xffffffff);
        write(msiCapability.address + 0x08, (msiAddress >> 32) & 0xffffffff);

        // write data
        write(msiCapability.address + 0x0c, static_cast<u32>(msiData));
    }

    else {
        // address is 32 bit
        write(msiCapability.address + 0x04, msiAddress & 0xffffffff);
        // write data
        write(msiCapability.address + 0x08, static_cast<u32>(msiData));
    }

    // enable MSI
    u32 newMSIHeader = (1 << 16);
    write(msiCapability.address, newMSIHeader);

    Logger::printFormat("[pcie] enabled MSI on vector 0x%x\n", vector);

}

void PCIDevice::enableBusMastering() {

    // enable it
    write(statusAndCommandOffset, read(statusAndCommandOffset) | (1 << 1) | (1 << 2) | (1 << 4));

}

void PCIDevice::disablePICInterrupts() {

    // disable it
    write(statusAndCommandOffset, read(statusAndCommandOffset) & ~(1 << 10));

}

void PCIDevice::dumpCapabilities() {

    // if no capabilities are found, return
    if(capabilities->size() == 0) return;

    // iterate through capability list and print all
    Logger::printFormat("[pcie]     dumping PCI device capabilities:\n");
    for(usz i = 0; i < capabilities->size(); i++) {
        const auto& capability = capabilities->get(i);
        Logger::printFormat("[pcie]        * at 0x%x - %s (0x%x)\n",
                            capability.address, getCapabilityReadableString(capability.type),
                            capability.type);
    }


}

const char *PCIDevice::getCapabilityReadableString(u8 capabilityID) {

    // switch on capability id
    // NOTE: IDs taken from https://pcisig.com/sites/default/files/files/PCI_Code-ID_r_1_11__v24_Jan_2019.pdf
    switch(capabilityID) {
        case 0x00: return "null capability";
        case 0x01: return "PCIPM";
        case 0x02: return "AGP";
        case 0x03: return "VPD";
        case 0x04: return "slot ID";
        case 0x05: return "MSI";
        case 0x06: return "CompactPCI hot swap";
        case 0x07: return "PCI-X";
        case 0x08: return "HyperTransport";
        case 0x09: return "vendor specific";
        case 0x0a: return "debug port";
        case 0x0b: return "CompactPCI central resource control";
        case 0x0c: return "PCI hot-plug";
        case 0x0d: return "PCI bridge subsystem vendor ID";
        case 0x0e: return "AGP 8x";
        case 0x0f: return "secure device";
        case 0x10: return "PCIe";
        case 0x11: return "MSI-X";
        case 0x12: return "SATA data/index configuration";
        case 0x13: return "AF";
        case 0x14: return "enhanced allocation";
        case 0x15: return "flattening portal bridge";
        default: return "reserved/undefined";
    }

}

u32 PCIDevice::read(u16 offset) {

    // use bus segment to do reads
    return segment->read(busNumber, deviceNumber, functionNumber, offset);

}

void PCIDevice::write(u16 offset, u32 value) {

    // use bus segment to do writes
    segment->write(busNumber, deviceNumber, functionNumber, offset, value);

}
