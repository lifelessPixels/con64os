
#include "physalloc.h"

void PhysicalAllocator::initialize()
{

    // get memory map from bootboot structure
    BootBoot::MemoryMapEntry *memoryMap = BootBoot::getStructure().memoryMap;
    usz entryCount = (BootBoot::getStructure().size - 128) / sizeof(BootBoot::MemoryMapEntry);

    // print memory map
    Logger::printFormat("[physalloc] entry count: %u\n", entryCount);
    for(u32 i = 0; i < entryCount; i++) {
        Logger::printFormat("[physalloc]   - [%u] type: %u, address: 0x%x, size: 0x%x\n", 
                            i + 1, static_cast<u8>(memoryMap[i].getType()), memoryMap[i].getAddress(), memoryMap[i].getSize());
    }

    // find decent-sized chunk for initial kernel heap
    u64 neededSize = (briefBitmapPages + largePageBitmapPages) * pageSize;
    BootBoot::MemoryMapEntry *found = nullptr;
    Logger::printFormat("[physalloc] trying to find suitable chunk of size 0x%x for page allocation bitmaps...\n", neededSize);
    for(u32 i = 0; i < entryCount; i++) {
        if(memoryMap[i].getAddress() < 0x100000) continue;
        if(!memoryMap[i].isFree()) continue;
        if(memoryMap[i].getSize() < neededSize) continue;

        // we found one, suitable chunk
        Logger::printFormat("[physalloc] found suitable chunk (%u) at 0x%x\n", i + 1, memoryMap[i].getAddress());
        found = &memoryMap[i];
        break;
    }

    // if not found, abort
    if(!found) {
        Logger::printFormat("[physalloc] could not find suitable chunk for page allocation bitmaps, aborting...");
        for(;;); // TODO: panic!
    }

    // if found, shrink chunk (or set is as unusable)
    briefBitmap = reinterpret_cast<u8*>(found->getAddress() + CPU::pagingBase);
    largePageBitmap = reinterpret_cast<AllocationBitmapEntry*>(found->getAddress() + pageSize + CPU::pagingBase);
    if(found->getSize() == neededSize) {
        found->setType(BootBoot::MemoryMapEntry::Type::used);
    }
    else {
        found->setAddress(found->getAddress() + neededSize);
        found->setSize(found->getSize() - neededSize);
    }

    // "zero-out" page allocation bitmap - assume all pages are allocated from the begining 
    for(u64 i = 0; i < maxLargePage; i++) setBriefBitmapEntry(i, BriefBitmapEntryType::FullySingleAllocated);
    for(u64 i = 0; i < maxLargePage; i++) setLargePageBitmapEntry(i, reservedProcessID, static_cast<u8>(AllocationBitmapEntryFlags::Allocated) | static_cast<u8>(AllocationBitmapEntryFlags::Reserved));
    
    // mark all available regions as unused
    for(u64 i = 0; i < entryCount; i++) {

        // skip unusable entries
        if(memoryMap[i].getType() != BootBoot::MemoryMapEntry::Type::free) continue;
        if(memoryMap[i].getAddress() < 0x100000) continue;
        if(memoryMap[i].getSize() < largePageSize) continue;
        u64 address = memoryMap[i].getAddress();
        u64 size = memoryMap[i].getSize();

        // align address and size to 1MiB chunks
        u64 toAlign = largePageSize - (address % largePageSize);
        if(toAlign != largePageSize) { 
            size -= toAlign;
            if(size < largePageSize) {
                
                continue;
            }
            size -= (size % largePageSize);
            address += toAlign;
        }

        Logger::printFormat("[physalloc] setting 0x%x (page start: 0x%x) (of size 0x%x) as free\n", address, address / largePageSize, size);

        // mark 2MiB pages as free
        u64 pageCount = size / largePageSize;
        u64 firstPage = address / largePageSize;
        for(u64 j = 0; j < pageCount; j++) {
            if(firstPage + j >= maxLargePage) break;
            setBriefBitmapEntry(firstPage + j, BriefBitmapEntryType::FullyFree);
            setLargePageBitmapEntry(firstPage + j, 0, 0);
        }

        // calculate how many pages are available
        freePagesCount += (size / pageSize) - pageCount;
        freeLargePagesCount += pageCount;

    }

    
    Logger::printFormat("[physalloc] free page count after initialization: %d\n", freePagesCount);
    Logger::printFormat("[physalloc] free large page count after initialization: %d\n", freeLargePagesCount);

}

void *PhysicalAllocator::allocatePage(u32 pid, bool large) {

    // ensure mutual exclusion
    ScopedSpinlock lock(allocatorSpinlock);

    // allocate "small" page
    if(!large) {
        
        // check if any page is available
        if(freePagesCount == 0) {
            Logger::printFormat("[physalloc] could not allocate page (no pages left), aborting...\n");
            for(;;); // TODO: panic!
        }

        // iterate and find partially free page or totally free page
        u64 firstFreePage = 0;
        u64 partiallyFreePage = 0;
        bool foundPartiallyFreePage = false;
        for(u64 i = 1; i < maxLargePage; i++) {
            
            // get entry type
            BriefBitmapEntryType type = getBriefBitmapEntry(i);

            // if there is free page and it's first, save it if needed later
            if(type == BriefBitmapEntryType::FullyFree && firstFreePage == 0) firstFreePage = i;
            if(type == BriefBitmapEntryType::PartiallyFree) {
                partiallyFreePage = i;
                foundPartiallyFreePage = true;
                break;
            }

        }

        // if partially allocated page was found, allocate page in it
        if(foundPartiallyFreePage) {

            // Logger::printFormat("[physalloc] allocating page in partially allocated large page 0x%x\n", partiallyFreePage);

            // get the pointer to allocation bitmap
            LargePageAllocationBitmap *bitmap = reinterpret_cast<LargePageAllocationBitmap*>(partiallyFreePage * largePageSize + CPU::pagingBase);

            // find free space
            for(u64 i = 0; i < 511; i++) {

                if((bitmap->AllocationEntries[i].Flags & static_cast<u8>(AllocationBitmapEntryFlags::Allocated)) == 0) {

                    // mark as used
                    bitmap->AllocationEntries[i].Flags = static_cast<u8>(AllocationBitmapEntryFlags::Allocated);
                    bitmap->AllocationEntries[i].ProcessID = pid;
                    bitmap->FreePages--;
                    freePagesCount--;

                    // if all pages are allocated, propagate this info to brief allocation bitmap
                    if(bitmap->FreePages == 0) setBriefBitmapEntry(partiallyFreePage, BriefBitmapEntryType::FullyPageAllocated);

                    // return page address
                    return reinterpret_cast<void*>(partiallyFreePage * largePageSize + (i + 1) * pageSize);

                }

            }

            // this should not happen
            Logger::printFormat("[physalloc] could not allocate page (loop overrun), aborting...\n");
            for(;;); // TODO: panic!

        }

        // no partially allocated page was found, allocate new
        else {

            // panic if no free large page was found
            if(firstFreePage == 0) {
                Logger::printFormat("[physalloc] could not allocate page (no large pages available), aborting...\n");
                for(;;); // TODO: panic!
            }

            // Logger::printFormat("[physalloc] allocating page in newly allocated large page 0x%x\n", firstFreePage);

            // otherwise, allocate new allocation bitmap
            LargePageAllocationBitmap *newBitmap = reinterpret_cast<LargePageAllocationBitmap*>(firstFreePage * largePageSize + CPU::pagingBase);
            for(u64 i = 1; i < 511; i++) newBitmap->AllocationEntries[i].Flags = 0;
            newBitmap->FreePages = 510;
            newBitmap->AllocationEntries[0].Flags = static_cast<u8>(AllocationBitmapEntryFlags::Allocated);
            newBitmap->AllocationEntries[0].ProcessID = pid;
            setBriefBitmapEntry(firstFreePage, BriefBitmapEntryType::PartiallyFree);
            freePagesCount--;
            freeLargePagesCount--;
            return reinterpret_cast<void*>(partiallyFreePage * largePageSize + pageSize);

        }

    }

    // allocate large page
    else {

        // panic if there is no page to be allocated
        if(freeLargePagesCount == 0) {
            Logger::printFormat("[physalloc] could not allocate page (no large pages left), aborting...\n");
            for(;;); // TODO: panic!
        }

        for(u64 i = 1; i < maxLargePage; i++) {
            
            // get entry type
            BriefBitmapEntryType type = getBriefBitmapEntry(i);

            // if there is free page, allocate it
            if(type == BriefBitmapEntryType::FullyFree) {
                
                // Logger::printFormat("[physalloc] allocating large page in page 0x%x\n", i);

                // mark page as used and set info
                setBriefBitmapEntry(i, BriefBitmapEntryType::FullySingleAllocated);
                setLargePageBitmapEntry(i, pid, static_cast<u8>(AllocationBitmapEntryFlags::Allocated));
                freeLargePagesCount--;

                // return page
                return reinterpret_cast<void*>(i * largePageSize);

            }

        }

        // this should not happen
        Logger::printFormat("[physalloc] could not allocate page (large page loop overrun), aborting...\n");
        for(;;); // TODO: panic!

    }

    // execution will not get here
    return nullptr;

}

void PhysicalAllocator::freePage(void *address) {

    // ensure mutual exclusion
    ScopedSpinlock lock(allocatorSpinlock);

    // convert pointer to value
    u64 convertedAddress = reinterpret_cast<u64>(address);

    // ignore invalid addresses
    if(convertedAddress >= maxLargePage * largePageSize) return;

    // index calculation
    u64 index = convertedAddress / largePageSize;

    // check page granularity
    if(convertedAddress % largePageSize == 0) {

        // free large page if it's not reserved
        u8 flags = getLargePageBitmapEntry(index).Flags;
        if((flags & static_cast<u8>(AllocationBitmapEntryFlags::Allocated)) > 0 && (flags & static_cast<u8>(AllocationBitmapEntryFlags::Reserved)) == 0) {

            // check if it's really singly allocated large page
            if(getBriefBitmapEntry(index) != BriefBitmapEntryType::FullySingleAllocated) return;

            // free page
            setBriefBitmapEntry(index, BriefBitmapEntryType::FullyFree);
            setLargePageBitmapEntry(index, 0, 0);
            freeLargePagesCount++;

        }

    } 

    // free "small" page
    else {

        // check brief entry type for sure
        BriefBitmapEntryType type = getBriefBitmapEntry(index);
        if(type != BriefBitmapEntryType::FullyPageAllocated && type != BriefBitmapEntryType::PartiallyFree) return;

        // get bitmap address and offset
        LargePageAllocationBitmap *bitmap = reinterpret_cast<LargePageAllocationBitmap*>(index * largePageSize + CPU::pagingBase);
        u64 offset = (convertedAddress - (index * largePageSize)) / pageSize;
        bitmap->AllocationEntries[offset].ProcessID = 0;
        bitmap->AllocationEntries[offset].Flags = 0;
        bitmap->FreePages++;
        freePagesCount++;

        // update brief bitmap accordingly
        if(bitmap->FreePages == 1) setBriefBitmapEntry(index, BriefBitmapEntryType::PartiallyFree);
        else if(bitmap->FreePages == 511) {
            setBriefBitmapEntry(index, BriefBitmapEntryType::FullyFree);
            freeLargePagesCount++;
        }

    }

}

void PhysicalAllocator::setBriefBitmapEntry(u64 pageIndex, BriefBitmapEntryType type) {
    
    // panic on too big page index
    if(pageIndex >= maxLargePage) {
        Logger::printFormat("[physalloc] could not set brief bitmap entry (index out of range), aborting...");
        for(;;); // TODO: panic!
    }

    // set accordingly
    u64 index = pageIndex / 4;
    u64 offset = (pageIndex % 4) * 2;
    u8 pageType = static_cast<u8>(type);
    briefBitmap[index] &= ~(0b11 << offset);
    briefBitmap[index] |= (pageType << offset);

}

void PhysicalAllocator::setLargePageBitmapEntry(u64 pageIndex, u32 pid, u8 flags) {

    // panic on too big page index
    if(pageIndex >= maxLargePage) {
        Logger::printFormat("[physalloc] could not set large page bitmap entry (index out of range), aborting...");
        for(;;); // TODO: panic!
    }

    // set values
    largePageBitmap[pageIndex].ProcessID = pid;
    largePageBitmap[pageIndex].Flags = flags;

}

PhysicalAllocator::BriefBitmapEntryType PhysicalAllocator::getBriefBitmapEntry(u64 pageIndex) {

    // panic on too big page index
    if(pageIndex >= maxLargePage) {
        Logger::printFormat("[physalloc] could not get brief bitmap entry (index out of range), aborting...");
        for(;;); // TODO: panic!
    }

    // return value
    u64 index = pageIndex / 4;
    u64 offset = (pageIndex % 4) * 2;
    return static_cast<BriefBitmapEntryType>((briefBitmap[index] >> offset) & 0b11);

}

PhysicalAllocator::AllocationBitmapEntry PhysicalAllocator::getLargePageBitmapEntry(u64 pageIndex) {

    // panic on too big page index
    if(pageIndex >= maxLargePage) {
        Logger::printFormat("[physalloc] could not get large page bitmap entry (index out of range), aborting...");
        for(;;); // TODO: panic!
    }

    // return value
    return largePageBitmap[pageIndex];

}

