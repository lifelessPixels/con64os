#include "vas.h"

VirtualMemoryObject::VirtualMemoryObject(u8 accessParameters, void *mappingAddress) {

    // create a list of all pages contained in this object
    pages = new List<void*>();

    // set all values
    flags = accessParameters;
    prefferedAddress = mappingAddress;
    
}

VirtualMemoryObject::~VirtualMemoryObject() {
    // free the list
    delete pages;
}

usz VirtualMemoryObject::objectSize() { return size; }
bool VirtualMemoryObject::largePageAligned() {return largePageAlignmentNeeded; }
void *VirtualMemoryObject::objectAddress() { return prefferedAddress; }
u8 VirtualMemoryObject::objectFlags() { return flags; }
List<void*> *VirtualMemoryObject::objectPages() { return pages; }

MMIOVirtualMemoryObject::MMIOVirtualMemoryObject(void *physicalAddress, usz length, void *mappingAddress) 
    : VirtualMemoryObject(writeable, mappingAddress) {
    
    // NOTE: physical address should always be page aligned to page size when calling this function
    // calculate all needed values
    bool largePages = (length > PhysicalAllocator::largePageSize);
    usz pageSize = largePages ? PhysicalAllocator::largePageSize : PhysicalAllocator::pageSize;
    usz pageCount = (length + (pageSize - 1)) / pageSize;
    usz startingAddress = reinterpret_cast<usz>(physicalAddress);

    // add all addresses to the list
    for(usz i = 0; i < pageCount; i++) {
        pages->appendBack(reinterpret_cast<void*>(startingAddress + i * pageSize));
        size += pageSize;
    }

    // set info about large pages
    largePageAlignmentNeeded = largePages;

}

MemoryBackedVirtualMemoryObject::MemoryBackedVirtualMemoryObject(usz length, bool disallowLargePages, void *mappingAddress, bool write, bool execute, bool cache, u32 pid)
    : VirtualMemoryObject((write ? writeable : 0) | (execute ? executable : 0) | (cache ? cacheable : 0), mappingAddress) {

    // check whether large pages are an option
    bool largePagesUsed = !disallowLargePages;
    if(mappingAddress != nullptr && (reinterpret_cast<u64>(mappingAddress) % PhysicalAllocator::largePageSize) != 0) largePagesUsed = false;
    if(length < (PhysicalAllocator::largePageSize)) largePagesUsed = false;

    // calculate count of pages needed to be allocated
    usz pageSize = largePagesUsed ? PhysicalAllocator::largePageSize : PhysicalAllocator::pageSize;
    usz pageCount = (length + (pageSize - 1)) / pageSize;

    // allocate pages and add them to the list
    for(usz i = 0; i < pageCount; i++) {
        void *newlyAllocatedPage = PhysicalAllocator::allocatePage(pid, largePagesUsed);
        pages->appendBack(newlyAllocatedPage);
        size += pageSize;
    }

    // set the flag if using large pages
    if(largePagesUsed) largePageAlignmentNeeded = true;

}

MemoryBackedVirtualMemoryObject::~MemoryBackedVirtualMemoryObject() {

    // free all allocated pages
    for(usz i = 0; i < pages->size(); i++) PhysicalAllocator::freePage(pages->get(i));

    // NOTE: the list will be removed in base class destructor

}

UncacheablePageVirtualMemoryObject::UncacheablePageVirtualMemoryObject(bool large, void *mappingAddress)
    : VirtualMemoryObject(writeable, mappingAddress) {

    // allocate page
    pages->appendBack(PhysicalAllocator::allocatePage(kernelPID, large));
    largePageAlignmentNeeded = large;
    size = (large) ? PhysicalAllocator::largePageSize : PhysicalAllocator::pageSize;
    
}

UncacheablePageVirtualMemoryObject::~UncacheablePageVirtualMemoryObject() {
    PhysicalAllocator::freePage(pages->get(0));
}

void *UncacheablePageVirtualMemoryObject::getPhysicalAddress() {
    return pages->get(0);
}
