#include "driver/arch/apic.h"

void LAPIC::initializeCoreLAPIC() {

    // get core id
    u8 apicID = CPU::getCoreAPICID();

    // enable LAPIC
    write(spuriousInterruptVectorOffset, 0x1ff); // spurious interrupt is 0xff, enable APIC

    // log core info
    Logger::printFormat("[apic] core %d - lapic initialized\n", apicID);

    // send EOI after initializing LAPIC (discard any pending interrupts in IOAPICS)
    sendEOI();

}

u8 LAPIC::getServicedInterruptVector() {

    // check all 8 ISRs 
    for(usz i = 0; i < 8; i++) {

        u32 bitfield = read(inServiceOffset + (i * 0x10));
        for(usz j = 0; j < 32; j++) {
            if(bitfield & (1 << j)) return (i * 32) + j;
        }

    }

    // otherwise return 0
    return 0;

}

void LAPIC::sendEOI() {

    // write something to EOI register
    write(eoiOffset, 0);

}

u32 LAPIC::read(u32 offset) {

    // check bounds
    if(offset >= 0x400) return 0;

    // return value
    u32 volatile *lapicRegister = reinterpret_cast<u32*>(reinterpret_cast<usz>(APIC::getLAPICAddress()) + offset);
    return *lapicRegister;

}

void LAPIC::write(u32 offset, u32 value) {

    // check bounds
    if(offset >= 0x400) return;

    // return value
    u32 volatile *lapicRegister = reinterpret_cast<u32*>(reinterpret_cast<usz>(APIC::getLAPICAddress()) + offset);
    *lapicRegister = value;

}


void IOAPIC::initialize(MMIOVirtualMemoryObject *mmioObject, void *mappedAddress, u32 globalSystemInterruptBase) {

    // multiple IOAPICs not supported yet
    if(initialized) {
        Logger::printFormat("[apic] multiple IOAPICs not yet supported, aborting...\n");
        for(;;); // TODO: panic! 
    }

    // set available pins
    availablePins = new List<u8>();

    // set values 
    object = mmioObject;
    registers = reinterpret_cast<u32*>(mappedAddress);
    globalSystemInterrupt = globalSystemInterruptBase;

    // get pin count
    u32 version = read(ioapicVersionIndex);
    redirectionEntryCount = ((version >> 16) & 0xff) + 1;
    Logger::printFormat("[apic] ioapic: version %d, supports %d pins\n", version & 0xff, redirectionEntryCount);

    // mask all entries
    for(usz i = 0; i < redirectionEntryCount; i++) {
        write(ioapicRedirectionBaseIndex + (i * 2) + 0, 0xff | (1 << 16));
        write(ioapicRedirectionBaseIndex + (i * 2) + 1, 0);

        // insert availabe pins outside of legacy PIC 0-15
        if(i > 15) availablePins->appendBack(static_cast<u8>(i));
    }

    // set initialized
    initialized = true;

}

bool IOAPIC::tryRegisterEntry(u8 entry, InterruptHandler handler, void *data) {

    // check whether input is available
    bool found = false;
    for(usz i = 0; i < availablePins->size(); i++) {
        if(availablePins->get(i) == entry) {
            availablePins->remove(i);
            found = true;
            break;
        }
    }
    if(!found) return false;

    u8 vector = Interrupts::reserveVector(handler, data);
    if(vector == 0) {
        Logger::printFormat("[apic] could not reserve interrupt vector for IOAPIC pin, aborting...\n");
        for(;;); // TODO: panic! 
    }

    // set redirection entry
    u64 redirectionEntry = static_cast<u64>(vector);
    redirectionEntry |= (static_cast<u64>(BootBoot::getStructure().bspID) << 56);
    write(ioapicRedirectionBaseIndex + (entry * 2) + 0, redirectionEntry & 0xffffffff);
    write(ioapicRedirectionBaseIndex + (entry * 2) + 1, (redirectionEntry >> 32) & 0xffffffff);
    return true;

}

u32 IOAPIC::read(u32 index) {
    
    // read the value
    registers[0] = index;
    return registers[4];

}

void IOAPIC::write(u32 index, u32 value) {

    // write the value
    registers[0] = index;
    registers[4] = value;

}


void APIC::initialize() {

    // get MADT table
    MADT *madt = reinterpret_cast<MADT*>(ACPI::getTableBySignature("APIC"));
    if(madt == nullptr) {
        Logger::printFormat("[apic] MADT table not found, aborting...\n");
        for(;;);
        // TODO: panic! 
    }

    // parse MADT table
    usz offset = 0;
    usz size = madt->length - sizeof(ACPI::Table);
    bool addressOverriden = false;

    // get lapic address
    lapicAddress = reinterpret_cast<void*>(madt->lapicAddress);

    Logger::printFormat("[apic] listing all entries in MADT: \n");
    while(offset < size) {

        // get entry
        MADTEntry *entry = reinterpret_cast<MADTEntry*>(&madt->data[offset]);

        // switch by type
        switch(entry->entryType) {
            
            // LAPIC
            case MADTEntryType::LAPICEntry: {

                // note: this information is not used anywhere - BOOTBOOT enables all cores by default, so it is not needed
                LAPICEntryStruct *lapic = reinterpret_cast<LAPICEntryStruct*>(entry);
                Logger::printFormat("[apic]   - LAPIC: acpi ID: %d, apic ID: %d, processor enabled? %b\n", 
                                    lapic->acpiProcessorID, lapic->apicID, (lapic->flags & 1) == 1);
                break;

            }

            // IOAPIC
            case MADTEntryType::IOAPICEntry: {

                IOAPICEntryStruct *ioapic = reinterpret_cast<IOAPICEntryStruct*>(entry);
                Logger::printFormat("[apic]   - IOAPIC: apic ID: %d, address: 0x%x, global interrupt base: %d\n", 
                                    ioapic->apicID, ioapic->address, ioapic->globalSystemInterruptBase);
                
                // map IOAPIC into virtual memory
                MMIOVirtualMemoryObject *object = new MMIOVirtualMemoryObject(reinterpret_cast<void*>(ioapic->address), 4096);
                void *ioapicMapped = VirtualAddressSpace::getKernelVirtualAddressSpace()->mapObject(object);
                Logger::printFormat("[apic]             mapped at: 0x%x\n", reinterpret_cast<usz>(ioapicMapped));
                IOAPIC::initialize(object, ioapicMapped, ioapic->globalSystemInterruptBase);

                break;

            }

            // IOAPIC source override
            case MADTEntryType::IOAPICSourceOverride: {

                IOAPICSourceOverrideStruct *sourceOverride = reinterpret_cast<IOAPICSourceOverrideStruct*>(entry);
                Logger::printFormat("[apic]   - IOAPIC Source Override: bus source: %d, irq source: %d, global interrupt: %d\n", 
                                    sourceOverride->busSource, sourceOverride->irqSource, sourceOverride->globalSystemInterrupt);

                break;

            }

            // IOAPIC NMI Source
            case MADTEntryType::IOAPICNMISource: {
                
                IOAPICNMISourceStruct *nmiSource = reinterpret_cast<IOAPICNMISourceStruct*>(entry);
                Logger::printFormat("[apic]   - IOAPIC NMI Source: nmi source: %d, global interrupt: %d  - not supported yet\n", 
                                    nmiSource->nmiSource, nmiSource->globalSystemInterrupt);
                break;

            }

            // LAPIC NMI
            case MADTEntryType::LAPICNMI: {

                LAPICNMIStruct *nmi = reinterpret_cast<LAPICNMIStruct*>(entry);
                Logger::printFormat("[apic]   - LAPIC NMI: acpi ID: %d, lint: %d\n", 
                                    nmi->acpiProcessorID, nmi->lintNumber);
                break;

            }

            // LAPIC Address Override
            case MADTEntryType::LAPICAddressOverride: {

                if(addressOverriden) {
                    Logger::printFormat("[apic]   - LAPIC Address Override: ignoring, multiple entries...\n");
                }
                else {
                    LAPICAddressOverrideStruct *addressOverride = reinterpret_cast<LAPICAddressOverrideStruct*>(entry);
                    Logger::printFormat("[apic]   - LAPIC Address Override: address: 0x%x\n", 
                                        addressOverride->lapicAddress);
                    lapicAddress = reinterpret_cast<void*>(addressOverride->lapicAddress);
                    addressOverriden = true;
                }
                break;
            }

            // LAPIC x2
            case MADTEntryType::LAPICx2Entry: {

                Logger::printFormat("[apic] LAPIC x2 found - not supported yet, aborting...\n");
                for(;;);
                // TODO: panic! 
                break;

            }

        }

        // increment offset by current entry size
        offset += entry->length;

    }

    // map LAPIC to virtual space of kernel
    MMIOVirtualMemoryObject *lapicObject = new MMIOVirtualMemoryObject(lapicAddress, 4096, nullptr);
    mappedLAPICAddress = VirtualAddressSpace::getKernelVirtualAddressSpace()->mapObject(lapicObject);
    Logger::printFormat("[apic] local APIC at address 0x%x (mapped at 0x%x)\n", reinterpret_cast<u64>(lapicAddress), reinterpret_cast<u64>(mappedLAPICAddress));

}

void *APIC::getLAPICAddress() { return mappedLAPICAddress; }

void APIC::disableLegacyPIC() {

    // mask all interrupts from legacy PIC
    PortIO::outputToPort8(0xa1, 0xff);
    PortIO::outputToPort8(0x21, 0xff);

}