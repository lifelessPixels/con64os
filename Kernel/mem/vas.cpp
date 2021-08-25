#include "vas.h"

// get needed symbols of kernel to set attributes of mappings
extern u64 kernelCodeStart;
extern u64 kernelCodeEnd;
extern u64 kernelRodataStart;
extern u64 kernelRodataEnd;

VirtualAddressSpace::VirtualAddressSpace() {

    // create allocation list
    allocationList = new List<VirtualMemoryRegion*>();

    // create table for PML4 entries if kernel address space was initialized (otherwise we are creating kernel address space)
    if(kernelAddressSpaceInitialized) {

        // allocate page
        cr3Value = allocateZeroedPage();
        mappingStructure = reinterpret_cast<PML4Entry*>(reinterpret_cast<usz>(cr3Value) + CPU::pagingBase);

        // copy kernel space PML4 entries to newly created address space
        for(usz i = 256; i < 512; i++) 
            if(kernelAddressSpace->mappingStructure[i].present == 1) 
                mappingStructure[i] = kernelAddressSpace->mappingStructure[i];

        // set allocation region accordingly
        VirtualMemoryRegion *newRegion = new VirtualMemoryRegion();
        newRegion->address = (2ull * 1024ull * 1024ull); // start allocations at 2MiB
        newRegion->size = 0x800000000000 - newRegion->address; // top of user's memory address space
        newRegion->type = VirtualMemoryRegion::Type::Free;
        newRegion->object = nullptr;
        allocationList->appendBack(newRegion);

    }

    // create kernel address space
    else {

        Logger::printFormat("[vas] kernel address space initialization...\n");
        Logger::printFormat("[vas] kernel .text: 0x%x - 0x%x\n", reinterpret_cast<usz>(&kernelCodeStart), reinterpret_cast<usz>(&kernelCodeEnd));
        Logger::printFormat("[vas] kernel .rodata: 0x%x - 0x%x\n", reinterpret_cast<usz>(&kernelRodataStart), reinterpret_cast<usz>(&kernelRodataEnd));

        // get cr3 value from register of CPU
        cr3Value = reinterpret_cast<void*>(CPU::readCR3());
        mappingStructure = reinterpret_cast<PML4Entry*>(reinterpret_cast<usz>(cr3Value) + CPU::pagingBase);

        // mark first 512 GiB of kernel address space to non-executable
        mappingStructure[256].executionDisable = 1;

        // set allocation region accordingly
        VirtualMemoryRegion *newRegion = new VirtualMemoryRegion();
        newRegion->address = CPU::pagingBase + (512ull * 1024ull * 1024ull * 1024ull); // start allocations at paging base + 512 GiB (where phys mem is mapped)
        newRegion->size =  0xffffff8000000000 - newRegion->address;; // -512 GiB from top of memory address space
        newRegion->type = VirtualMemoryRegion::Type::Free;
        newRegion->object = nullptr;
        allocationList->appendBack(newRegion);

        // save address space
        kernelAddressSpace = this;
        kernelAddressSpaceInitialized = true;

    }

}

void *VirtualAddressSpace::mapObject(VirtualMemoryObject *object) { 

    // lock spinlock
    ScopedSpinlock lock(spinlock);

    // get info about object
    usz objectPrefferedAddress = reinterpret_cast<usz>(object->objectAddress());
    usz objectSize = object->objectSize();

    // if no preffered address is set, find first suitable chunk and split it
    if(!objectPrefferedAddress) {

        // iterate and find suitable chunk
        for(usz i = 0; i < allocationList->size(); i++) {

            VirtualMemoryRegion *current = allocationList->get(i);
            if(current->type != VirtualMemoryRegion::Type::Free) continue;

            // potentially there's suitable region, find out if alignment is needed
            usz regionSize = current->size;
            usz regionAddress = current->address;
            if(object->largePageAligned() && (current->address % PhysicalAllocator::largePageSize) != 0) {

                usz difference = PhysicalAllocator::largePageSize - (current->address % PhysicalAllocator::largePageSize);
                regionSize -= difference;
                regionAddress += difference;

                // if aligned size is suitable, split region singly or doubly
                if(regionSize >= objectSize) {

                    // firstly, create region of alignment
                    VirtualMemoryRegion *alignmentRegion = new VirtualMemoryRegion();
                    alignmentRegion->address = current->address;
                    alignmentRegion->size = difference;
                    alignmentRegion->object = nullptr;
                    alignmentRegion->type = VirtualMemoryRegion::Type::Free;

                    // check if additional region after current is needed
                    VirtualMemoryRegion *additionalRegion = nullptr;
                    if(regionSize > objectSize) {

                        additionalRegion = new VirtualMemoryRegion();
                        additionalRegion->address = regionAddress + objectSize;
                        additionalRegion->size = regionSize - objectSize;
                        additionalRegion->object = nullptr;
                        additionalRegion->type = VirtualMemoryRegion::Type::Free;

                    }

                    // change current region 
                    current->address = regionAddress;
                    current->size = objectSize;
                    current->object = object;
                    current->type = VirtualMemoryRegion::Type::Allocated;

                    // if additional region after exists, add it
                    if(additionalRegion != nullptr) allocationList->insertAt(additionalRegion, i + 1);

                    // add alignment region
                    allocationList->insertAt(alignmentRegion, i);

                    // map current region
                    doMapping(current);
                    return reinterpret_cast<void*>(current->address);

                }

            }

            // otherwise object is large page aligned, but not alignment is needed or no large page alignment is needed - just add it
            else {
                
                // check size
                if(regionSize >= objectSize) {

                    // check if additional region after current is needed
                    VirtualMemoryRegion *additionalRegion = nullptr;
                    if(regionSize > objectSize) {

                        additionalRegion = new VirtualMemoryRegion();
                        additionalRegion->address = regionAddress + objectSize;
                        additionalRegion->size = regionSize - objectSize;
                        additionalRegion->object = nullptr;
                        additionalRegion->type = VirtualMemoryRegion::Type::Free;

                    }

                    // change current region 
                    current->address = regionAddress;
                    current->size = objectSize;
                    current->object = object;
                    current->type = VirtualMemoryRegion::Type::Allocated;

                    // if additional region after exists, add it
                    if(additionalRegion != nullptr) allocationList->insertAt(additionalRegion, i + 1);

                    // map current region
                    doMapping(current);
                    return reinterpret_cast<void*>(current->address);

                }

            }

        }

        // no suitable region found, return nullptr
        return nullptr;

    }

    // TODO: not supported yet
    else return nullptr;

}

void VirtualAddressSpace::adjustKernelMemory() {

    // get paging structure
    PML4Entry *pml4 = reinterpret_cast<PML4Entry*>(CPU::readCR3());

    // move 0th PML4 entry to 256th
    pml4[256].value = pml4[0].value;
    pml4[0].value = 0ull;

    // reload mappings
    CPU::writeCR3(reinterpret_cast<u64>(pml4));

}

VirtualAddressSpace *VirtualAddressSpace::getKernelVirtualAddressSpace() {
    return kernelAddressSpace;
}

void *VirtualAddressSpace::allocateZeroedPage() {

    // allocate page
    void *page = PhysicalAllocator::allocatePage(kernelPID);

    // zero-out page
    usz *array = reinterpret_cast<usz*>(reinterpret_cast<usz>(page) + CPU::pagingBase);
    usz count = PhysicalAllocator::pageSize / sizeof(usz);
    for(usz i = 0; i < count; i++) array[i] = 0ull;

    // return page
    return page;

}

void *VirtualAddressSpace::getCR3() { return cr3Value; }

void *VirtualAddressSpace::getMappingEntry(void *address, bool large, bool create) {

    // firstly, split address into pieces
    usz convertedAddress = reinterpret_cast<usz>(address);
    convertedAddress >>= 12; // discard offset, it is not needed
    usz ptIndex = convertedAddress & 0b111111111;
    convertedAddress >>= 9;
    usz pdIndex = convertedAddress & 0b111111111;
    convertedAddress >>= 9;
    usz pdptIndex = convertedAddress & 0b111111111;
    convertedAddress >>= 9;
    usz pml4Index = convertedAddress & 0b111111111;

    // go through paging structure and get corresponding entry
    PML4Entry *pml4Entry = &mappingStructure[pml4Index];
    if(!pml4Entry->present && !create) return nullptr;
    else if(!pml4Entry->present) {

        // create new PDPT and set PML4 entry accordingly 
        void *newPDPT = allocateZeroedPage();
        // NOTE: assume that access flags in mapped pages are always set in lowest entries (in PD and PT)
        pml4Entry->address = reinterpret_cast<usz>(newPDPT) >> 12;
        pml4Entry->present = 1;
        pml4Entry->writeEnable = 1;

    }

    PDPTEntry *pdptEntry = &reinterpret_cast<PDPTEntry*>((pml4Entry->address << 12) + CPU::pagingBase)[pdptIndex];
    if(!pdptEntry->present && !create) return nullptr;
    else if(!pdptEntry->present) {

        // create new PD and set PDPT entry accordingly 
        void *newPD = allocateZeroedPage();
        pdptEntry->address = reinterpret_cast<usz>(newPD) >> 12;
        pdptEntry->present = 1;
        pdptEntry->writeEnable = 1;

    }

    PDEntry *pdEntry = &reinterpret_cast<PDEntry*>((pdptEntry->address << 12) + CPU::pagingBase)[pdIndex];
    if(large) return reinterpret_cast<void*>(pdEntry); // if the reference to large page entry is needed, return it now
    if(!pdEntry->ptReference.present && !create) return nullptr;
    else if(!pdEntry->ptReference.present) {

        // create new PT and set PD entry accordingly 
        void *newPT = allocateZeroedPage();
        pdEntry->ptReference.address = reinterpret_cast<usz>(newPT) >> 12;
        pdEntry->ptReference.present = 1;
        pdEntry->ptReference.writeEnable = 1;

    }

    // calculate address and return according entry
    return reinterpret_cast<void*>(&reinterpret_cast<PTEntry*>((pdEntry->ptReference.address << 12) + CPU::pagingBase)[ptIndex]);

}

void VirtualAddressSpace::doMapping(VirtualMemoryRegion *region) {

    // get relevant information
    VirtualMemoryObject *object = region->object;
    usz regionStart = region->address;
    bool largePages = object->largePageAligned();
    usz pageSize = largePages ? PhysicalAllocator::largePageSize : PhysicalAllocator::pageSize;
    usz pageCount = region->size / pageSize;
    // Logger::printFormat("[vas] mapping region of size 0x%x at 0x%x\n", region->size, region->address);
    List<void*> *pages = object->objectPages();

    if(pages->size() != pageCount) {
        Logger::printFormat("[vas] page counts do not match (%d vs %d), aborting...\n", pages->size(), pageCount);
        for(;;);
        // TODO: panic! 
    }

    // map pages
    u8 flags = object->objectFlags();
    for(usz i = 0; i < pageCount; i++) {

        // get entry
        void *entry = getMappingEntry(reinterpret_cast<void*>(regionStart + (i * pageSize)), largePages, true);
        void *page = pages->get(i);

        if(largePages) {

            // get reference to the entry
            PDEntry *pdEntry = reinterpret_cast<PDEntry*>(entry);

            // fill entry
            pdEntry->largePageReference.present = 1;
            pdEntry->largePageReference.pageSize = 1;
            pdEntry->largePageReference.writeEnable = (flags & VirtualMemoryObject::writeable) ? 1 : 0;
            pdEntry->largePageReference.executionDisable = (flags & VirtualMemoryObject::executable) ? 0 : 1;
            pdEntry->largePageReference.cacheDisable = (flags & VirtualMemoryObject::cacheable) ? 0 : 1;
            pdEntry->value |= (reinterpret_cast<u64>(page) & ~(0x1ffff));

        }

        else {

            // get reference to the entry
            PTEntry *ptEntry = reinterpret_cast<PTEntry*>(entry);

            // fill entry
            ptEntry->present = 1;
            ptEntry->writeEnable = (flags & VirtualMemoryObject::writeable) ? 1 : 0;
            ptEntry->executionDisable = (flags & VirtualMemoryObject::executable) ? 0 : 1;
            ptEntry->cacheDisable = (flags & VirtualMemoryObject::cacheable) ? 0 : 1;
            ptEntry->value |= (reinterpret_cast<u64>(page) & ~(0xfff));

        }

    }

}