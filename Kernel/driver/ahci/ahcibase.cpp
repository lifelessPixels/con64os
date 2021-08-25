#include "driver/ahci/ahcibase.h"

AHCI::AHCI(PCIDevice *device) {

    // save device
    pciDevice = device;

    // enable bus mastering and address space access (and disable PIC interrupts)
    pciDevice->enableBusMastering();
    pciDevice->disablePICInterrupts();

    // get ABAR from BAR5 and map it into kernel memory space
    void *physicalAddress = reinterpret_cast<void*>(pciDevice->getBARValue(5) & ~(0x1fff));
    vmObject = new MMIOVirtualMemoryObject(physicalAddress, 8192, nullptr);
    abar = reinterpret_cast<ABAR*>(VirtualAddressSpace::getKernelVirtualAddressSpace()->mapObject(vmObject));
    if(!abar) {
        Logger::printFormat("[ahci] could not map AHCI's ABAR to kernel address space, aborting...\n");
        for(;;);
        // TODO: panic! 
    }

    // perform BIOS/OS handoff if supported
    if(abar->hostCapabilitiesExtended & 1) {

        Logger::printFormat("[ahci]   - BIOS/OS handoff procedure started...\n");

        // set os ownership flag
        abar->biosHandoff = abar->biosHandoff | (1 << 1);

        // spin until BIOS releases AHCI
        while(abar->biosHandoff & 1);
        Logger::printFormat("[ahci]   - BIOS/OS handoof procedure ended successfully\n");

    }
    else Logger::printFormat("[ahci]   - AHCI does not support BIOS/OS handoff procedure, skipping...\n");

    // get basic info about AHCI
    u32 hbaCapabilities = abar->hostCapabilities;
    numberOfPorts = (hbaCapabilities & 0b11111) + 1; 
    numberOfCommandSlots = ((hbaCapabilities >> 8) & 0b11111) + 1;
    supports64Bit = (hbaCapabilities & (1 << 31));
    staggeredSpinUp = (hbaCapabilities & (1 << 27));
    Logger::printFormat("[ahci]   - initializing AHCI (version: 0x%x) - no of ports: %u, 64-bit?: %b, staggered spin-up: %b, command slots: %d\n", 
                        abar->version, numberOfPorts, supports64Bit, staggeredSpinUp, numberOfCommandSlots);
    if(!supports64Bit) {
        Logger::printFormat("[ahci]   - AHCI does not support 64-bit addressing, could not initialize...\n");
        return;
    }
    if(staggeredSpinUp) {
        Logger::printFormat("[ahci]   - AHCI requires manual spin-up of devices which is not yet supported, could not initialize...\n");
        return;
    }   

    // reset the controller
    abar->globalHostControl = (1 << 0);
    while(abar->globalHostControl & 1);
    Logger::printFormat("[ahci]   - controller reset successfully\n");

    // enable MSI, allocate vectors and enable interrupts
    u8 vector = Interrupts::reserveMSIVector(&AHCI::interruptHandler, reinterpret_cast<void*>(this));
    if(vector == 0) {
        Logger::printFormat("[ahci]   - could not allocate interrupt vector, aborting...\n");
        for(;;); // TODO: panic!
    }
    pciDevice->enableMSI(vector);

    // enable AHCI mode and interrupts
    abar->globalHostControl = abar->globalHostControl | (1 << 1) | (1 << 31);

    // setup all ports of HBA
    portInformation = new PortInfo[numberOfPorts];
    drives = new List<PortInfo*>();
    for(u8 i = 0; i < numberOfPorts; i++) {

        // check whether port is actually implemented
        if((abar->portsImplemented & (1 << i)) == 0) continue;
        Logger::printFormat("[ahci]   - port %d is implemented, creating memory spaces...\n", i);

        // initialize port
        if(!initializePort(i)) continue;

        // if port initialized successfully add it to the list
        drives->appendBack(&portInformation[i]);

    }

    // identify devices
    identifyDevices();

    // set AHCI initialized successfully
    initialized = true;
    
    for(usz i = 0; i < drives->size(); i++) {
        PortInfo *port = drives->get(i);
        AHCIBlockDevice *device = new AHCIBlockDevice(this, port->portNumber);
        blockDevices->appendBack(device);
    }

}

bool AHCI::initializedCorrectly() { return initialized; }

usz AHCI::getSectorCount(u8 port) {
    if(portInformation[port].identified) return portInformation[port].sectorCount;
    return 0;
}

bool AHCI::readSectors(u8 port, usz sectorStart, usz sectorCount, VirtualMemoryObject *buffer, EventHandler handler, void *handlerData) {

    // just issue a command
    return issueCommand(port, ataCommandReadDMAEx, sectorCount, sectorStart, true, false, buffer, handler, handlerData);

}

bool AHCI::initializePort(u8 portNumber) {
    
    // create a timer, useful later
    Timer *timer = new Timer();

    // create received FIS memory page
    UncacheablePageVirtualMemoryObject *receivedFISObject = new UncacheablePageVirtualMemoryObject();
    portInformation[portNumber].receivedFISObject = receivedFISObject;
    void *mappedReceivedFIS = VirtualAddressSpace::getKernelVirtualAddressSpace()->mapObject(receivedFISObject);
    portInformation[portNumber].receivedFISAddress = mappedReceivedFIS;
    if(!mappedReceivedFIS) {
        Logger::printFormat("[ahci]   - could not allocate address space for received FIS page, aborting...\n");
        return false;
    }

    // zero-out received FIS region
    u64 *page = reinterpret_cast<u64*>(mappedReceivedFIS);
    for(usz i = 0; i < 512; i++) page[i] = 0ull;

    // create command list
    UncacheablePageVirtualMemoryObject *commandListObject = new UncacheablePageVirtualMemoryObject();
    portInformation[portNumber].commandListObject = commandListObject;
    void *mappedCommandList = VirtualAddressSpace::getKernelVirtualAddressSpace()->mapObject(commandListObject);
    portInformation[portNumber].commandList = reinterpret_cast<CommandHeader*>(mappedCommandList);
    if(!mappedCommandList) {
        Logger::printFormat("[ahci]   - could not allocate address space for command list page, aborting...\n");
        return false;
    }

    // zero-out received FIS region
    page = reinterpret_cast<u64*>(mappedCommandList);
    for(usz i = 0; i < 512; i++) page[i] = 0ull;

    // create command tables for every command
    for(usz i = 0; i < numberOfCommandSlots; i++) {

        // create object and map it
        UncacheablePageVirtualMemoryObject *commandTableObject = new UncacheablePageVirtualMemoryObject();
        portInformation[portNumber].commandTableObjects[i] = commandTableObject;
        void *mappedCommandTable = VirtualAddressSpace::getKernelVirtualAddressSpace()->mapObject(commandTableObject);
        portInformation[portNumber].mappedCommandTables[i] = reinterpret_cast<CommandTable*>(mappedCommandTable);
        if(!mappedCommandTable) {
            Logger::printFormat("[ahci]   - could not allocate address space for command table page, aborting...\n");
            return false;
        }
        
        // zero-out command table
        page = reinterpret_cast<u64*>(mappedCommandTable);
        for(usz i = 0; i < 512; i++) page[i] = 0ull;

    }

    // set port number
    portInformation[portNumber].portNumber = portNumber;

    // all spaces created, initialize port - fill command list accordingly
    for(usz i = 0; i < numberOfCommandSlots; i++) {

        volatile CommandHeader *commandHeader = &portInformation[portNumber].commandList[i];
        u64 commandTableAddress = reinterpret_cast<u64>(portInformation[portNumber].commandTableObjects[i]->getPhysicalAddress());
        commandHeader->prdtLength = 128;
        commandHeader->commandTableBaseAddress = commandTableAddress & 0xffffffff;
        commandHeader->commandTableBaseAddressUpper = (commandTableAddress >> 32) & 0xffffffff;

    }

    // fill info about command list and received FIS
    u64 commandListAddress = reinterpret_cast<u64>(commandListObject->getPhysicalAddress());
    abar->ports[portNumber].commandListBaseAddress = commandListAddress & 0xffffffff;
    abar->ports[portNumber].commandListBaseAddressUpper = (commandListAddress >> 32)  & 0xffffffff;
    u64 receivedFISAddress = reinterpret_cast<u64>(receivedFISObject->getPhysicalAddress());
    abar->ports[portNumber].fisBaseAddress = receivedFISAddress & 0xffffffff;
    abar->ports[portNumber].fisBaseAddressUpper = (receivedFISAddress >> 32) & 0xffffffff;

    // enable receiving FISes
    abar->ports[portNumber].commandAndStatus = abar->ports[portNumber].commandAndStatus | (1 << 4);

    // reset the port - make controller issue COMRESET signal to reinitialize drive
    abar->ports[portNumber].sataControl = abar->ports[portNumber].sataControl | (1 << 0);
    
    // according to specification, at least 1ms must pass to be sure that COMRESET was sent
    timer->wait(2); // wait a bit longer than needed

    // deassert DET bit
    abar->ports[portNumber].sataControl = abar->ports[portNumber].sataControl & ~(1 << 0);

    // wait until communication with the device is estabilished or timeout after 100ms
    timer->nonBlockingWait(100);
    while((abar->ports[portNumber].sataStatus & 0x0f) != 3 && !timer->wasFired());
    if((abar->ports[portNumber].sataStatus & 0x0f) != 3) {
        Logger::printFormat("[ahci]   - no device to estabilish port communication\n");
        timer->disableNonBlockingWait();
        return false;
    }
    Logger::printFormat("[ahci]   - communication on port %d estabilished\n", portNumber);
    timer->disableNonBlockingWait();

    // clear sata errors generated in a process of port reset
    abar->ports[portNumber].sataError = 0xffffffff;

    // read signature
    u32 signature = abar->ports[portNumber].signature;
    Logger::printFormat("[ahci]   - attached device signature: 0x%x\n", signature);
    if(signature != 0x00000101) {
        Logger::printFormat("[ahci]   - device type not supported...\n");
        return false;
    }

    // enable interrupts on device and start port operation
    abar->ports[portNumber].interruptStatus = 0xffffffff;
    abar->ports[portNumber].interruptEnable = 0xffffffff;
    abar->ports[portNumber].commandAndStatus = abar->ports[portNumber].commandAndStatus | (1 << 0);

    // set fields in port information - set free commands slots
    u32 freeCommandSlots = 0;
    for(usz i = 0; i < 32; i++) {
        if(i >= numberOfCommandSlots) freeCommandSlots |= (1 << i);
    }
    portInformation[portNumber].commandsInUse = freeCommandSlots;
    portInformation[portNumber].sectorSize = 512;

    Logger::printFormat("[ahci]   - command list:  0x%x (0x%x)\n", reinterpret_cast<u64>(mappedCommandList), reinterpret_cast<u64>(commandListObject->getPhysicalAddress()));
    Logger::printFormat("[ahci]   - command table: 0x%x (0x%x)\n", reinterpret_cast<u64>(portInformation[portNumber].mappedCommandTables[0]), reinterpret_cast<u64>(portInformation[portNumber].commandTableObjects[0]->getPhysicalAddress()));

    return true;



}

void AHCI::identifyDevices() {

    // for all ports with attached drives, send identify command
    for(usz i = 0; i < drives->size(); i++) {
        PortInfo *port = drives->get(i);
        portInformation[i].identifyObject = new MemoryBackedVirtualMemoryObject(PhysicalAllocator::pageSize, true);
        portInformation[i].mappedIdentifyData = VirtualAddressSpace::getKernelVirtualAddressSpace()->mapObject(portInformation[i].identifyObject);
        if(!portInformation[i].mappedIdentifyData) {
            Logger::printFormat("[ahci]   - could not map identify data, aborting...\n");
            for(;;); // TODO: panic!
        }
        Logger::printFormat("[ahci]   - identifying port %d\n", port->portNumber);
        
        issueCommand(port->portNumber, ataCommandIdentify, 1, 0, false, false, portInformation[i].identifyObject, nullptr, nullptr);
        
    }

}

bool AHCI::issueCommand(u8 port, u8 command, u16 transferSectors, usz accessSector, bool mediaAccess, bool write, VirtualMemoryObject *data, EventHandler handler, void *handlerData) {

    // lock port's spinlock
    ScopedSpinlock lock(portInformation[port].portSpinlock);

    // check if command could be issued
    if(portInformation[port].commandsInUse == 0xffffffff) {
        // TODO: should be added to the pending transfers queue
        return false;
    }

    // check size (max 128 prdt, one page per entry)
    usz sectorSize = portInformation[port].sectorSize;
    if((transferSectors * sectorSize) > (128 * PhysicalAllocator::pageSize)) return false;
    // TODO: make it smarter (like split into two or more commands maybe?)

    // create request
    Request newRequest;
    newRequest.count = transferSectors;
    newRequest.sector = accessSector;
    newRequest.write = write;
    newRequest.handler = handler;
    newRequest.handlerData = handlerData;

    // find free command slot
    usz slot = 0;
    bool found = false;
    for(usz i = 0; i < numberOfCommandSlots; i++) {
        if(!(portInformation[port].commandsInUse & (1 << i))) {
            slot = i;
            found = true;
            break;
        }
    }
    if(!found) {
        Logger::printFormat("[ahci] weirdly there is no free command slot (after check), aborting...\n");
        for(;;); // TODO: panic!
    }


    // configure command slot
    volatile CommandHeader *commandSlot = &portInformation[port].commandList[slot];
    commandSlot->commandFISLength = sizeof(CommandFIS) / sizeof(u32);
    commandSlot->write = write;
    u32 prdtLength = ((transferSectors * sectorSize) + (PhysicalAllocator::pageSize - 1)) / PhysicalAllocator::pageSize;
    commandSlot->prdtLength = prdtLength;

    // configure command FIS
    volatile CommandTable *commandTable = portInformation[port].mappedCommandTables[slot];
    volatile CommandFIS *fis = reinterpret_cast<volatile CommandFIS*>(&commandTable->commandFIS);
    fis->fisType = 0x27; // register FIS - host to device
    fis->command = command;
    fis->isCommand = 1;
    if(mediaAccess) {
        fis->lba0 = (accessSector >> 0) & 0xff;
        fis->lba1 = (accessSector >> 8) & 0xff;
        fis->lba2 = (accessSector >> 16) & 0xff;
        fis->lba3 = (accessSector >> 24) & 0xff;
        fis->lba4 = (accessSector >> 32) & 0xff;
        fis->lba5 = (accessSector >> 40) & 0xff;
    }
    else {
        fis->lba0 = 0;
        fis->lba1 = 0;
        fis->lba2 = 0;
        fis->lba3 = 0;
        fis->lba4 = 0;
        fis->lba5 = 0;
    }

    if(mediaAccess) fis->device = (1 << 6);
    else fis->device = 0;

    if(mediaAccess) {
        fis->countLow = transferSectors & 0xff;
        fis->countHigh = (transferSectors >> 8) & 0xff;
    }
    else {
        fis->countLow = 0;
        fis->countHigh = 0;
    }

    

    // configure command PRDT
    for(usz i = 0; i < prdtLength; i++) {
        u64 pageAddress = reinterpret_cast<u64>(data->objectPages()->get(i));
        commandTable->physicalRegionDescriptorTable[i].byteCount = PhysicalAllocator::pageSize - 1;
        commandTable->physicalRegionDescriptorTable[i].dataBaseAddress = pageAddress & 0xffffffff;
        commandTable->physicalRegionDescriptorTable[i].dataBaseAddressUpper = (pageAddress >> 32) & 0xffffffff;
        commandTable->physicalRegionDescriptorTable[i].interrupt = 1;
    }

    // set info
    portInformation[port].currentRequests[slot] = newRequest;
    portInformation[port].commandsInUse |= (1 << slot);

    // issue the command
    abar->ports[port].commandIssue = (1 << slot);
    Logger::printFormat("[ahci] issued 0x%x command to port %d, to slot %d\n", command, port, slot);

    return true;
    
}

void AHCI::handleInterrupt() {


    // check on which port the interrupt was triggerred
    for(usz i = 0; i < numberOfPorts; i++) {

        if(!(abar->interruptStatus & (1 << i))) continue;

        // lock port spinlock
        ScopedSpinlock lock(portInformation[i].portSpinlock);

        // if port was not identified, identify port
        if(!portInformation[i].identified) {

            // get all needed identify data
            u16 *identifyData = reinterpret_cast<u16*>(portInformation[i].mappedIdentifyData);
            u64 sectorCount = (static_cast<u64>(identifyData[100]) << 0) | (static_cast<u64>(identifyData[101]) << 16) | (static_cast<u64>(identifyData[102]) << 32) | (static_cast<u64>(identifyData[103]) << 48);
            portInformation[i].sectorCount = sectorCount;
            Logger::printFormat("[ahci] identified device has %d sectors\n", sectorCount);

            // set info about identification
            portInformation[i].identified = true;

        }

        // normal command
        else {
            
            // TODO: do error checking!

            // fire callbacks of all completed commands
            for(usz j = 0; j < 32; j++) {

                // command completed
                if(!(abar->ports[i].commandIssue & (1 << j)) && (portInformation[i].commandsInUse & (1 << j))) {

                    // clear command in use
                    portInformation[i].commandsInUse &= ~(1 << j);

                    // fire callback
                    portInformation[i].currentRequests[j].handler(portInformation[i].currentRequests[j].handlerData);

                }

            }

        }

        // clear interrupt status
        abar->ports[i].interruptStatus = 0xffffffff;


    }

    // clear interrupt flags globally
    abar->interruptStatus = 0xffffffff;

}

void AHCI::initialize() {

    // create list of all AHCI devices
    devices = new List<AHCI*>();
    blockDevices = new List<IBlockDevice*>();

    // find all PCI devices with relevant IDs
    List<PCIDevice*> *ahciPCIDevices = new List<PCIDevice*>();
    PCIe::getDevicesByClassCodes(ahciPCIDevices, 0x01, 0x06, 0x01);

    // check whether any AHCI devices were found
    if(ahciPCIDevices->size() == 0) {
        Logger::printFormat("[ahci] no AHCI devices were found\n");
        return;
    }
    Logger::printFormat("[ahci] AHCI devices count: %u\n", ahciPCIDevices->size());

    // initialize all of found AHCIs
    for(usz i = 0; i < ahciPCIDevices->size(); i++) {

        // get accompanied PCI device
        PCIDevice *pciDevice = ahciPCIDevices->get(i);

        // check whether AHCI supports MSI
        if(!pciDevice->supportsMSI()) {
            Logger::printFormat("[ahci] found device without MSI support, ignoring...\n");
            continue;
        }

        // create AHCI for it
        Logger::printFormat("[ahci] trying to initialize AHCI number %u\n", i);
        AHCI *ahci = new AHCI(pciDevice);
        if(ahci->initializedCorrectly()) devices->appendBack(ahci);
        else delete ahci;

    }

    // destroy the list of found AHCIs
    delete ahciPCIDevices;

}

List<IBlockDevice*> *AHCI::getBlockDevices() { return blockDevices; }

void AHCI::interruptHandler(void *data, u32) {

    // data is AHCI object, convert it and handle interrupt
    AHCI *ahci = reinterpret_cast<AHCI*>(data);
    ahci->handleInterrupt();

}   

AHCIBlockDevice::AHCIBlockDevice(AHCI *underlyingAHCI, u8 portNumber) : ahci(underlyingAHCI), port(portNumber) {}

bool AHCIBlockDevice::isWriteable() {
    return false;
}

usz AHCIBlockDevice::sectorCount() {
    return ahci->getSectorCount(port);
}

bool AHCIBlockDevice::read(usz sector, usz count, VirtualMemoryObject *buffer, EventHandler handler, void *handlerData) {
    return ahci->readSectors(port, sector, count, buffer, handler, handlerData);
}

bool AHCIBlockDevice::write(usz, usz, VirtualMemoryObject *, EventHandler, void *) {
    return false;
}
