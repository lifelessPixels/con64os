
#include "mem/heap.h"

void Heap::initialize() {

    // create first chunk in the list
    allocateAndAppendNewChunk();

    Logger::printFormat("[heap] initialized with 2MiB chunk\n");

}

void *Heap::allocate(usz size) {

    // lock spinlock
    ScopedSpinlock lock(heapSpinlock);

    // adjust allocation size
    usz adjusted = ((size + (allocationAlignment - 1)) / allocationAlignment) * allocationAlignment;

    // check size of allocation
    if(adjusted > fullPageAllocationSize) {
        Logger::printFormat("[heap] allocation size to big, aborting...\n");
        for(;;);
        // TODO: panic! 
    }

    // try to allocate chunk
    ChunkInfoBlock *currentChunk = chunkListFirst;
    while(currentChunk != nullptr) {
        void *address = findAllocation(currentChunk, adjusted);
        if(address != nullptr) return address;
        currentChunk = currentChunk->next;
    }

    // in this case it is needed to allocate new block and allocate 
    ChunkInfoBlock *newChunk = allocateAndAppendNewChunk();
    void *address = findAllocation(newChunk, adjusted);
    if(address == nullptr) {
        Logger::printFormat("[heap] allocation not successful at 0x%x (should not happen), aborting...\n", reinterpret_cast<usz>(newChunk));
        for(;;);
        // TODO: panic! 
    }
    return address;

}

void Heap::free(void *address) {

    // lock spinlock
    ScopedSpinlock lock(heapSpinlock);

    // get addresses to AllocationDescriptor and ChunkInfoBlock
    ChunkInfoBlock *chunk = reinterpret_cast<ChunkInfoBlock*>(reinterpret_cast<u64>(address) & ~0x1fffff);
    AllocationDescriptor *descriptor = reinterpret_cast<AllocationDescriptor*>(reinterpret_cast<u64>(address) - sizeof(AllocationDescriptor));

    // change allocation type to free
    descriptor->type = AllocationType::Free;

    // try to merge with previous and next descriptors
    AllocationDescriptor *previous = descriptor->previous;
    AllocationDescriptor *next = descriptor->next;
    if(previous != nullptr && previous->type == AllocationType::Free) {

        // merge with previous
        previous->size += (descriptor->size + sizeof(AllocationDescriptor));
        previous->next = next;
        if(next != nullptr) {
            next->previous = previous;
        }
        descriptor = previous;

    }
    if(next != nullptr && next->type == AllocationType::Free) {

        // merge with next
        descriptor->size += (next->size + sizeof(AllocationDescriptor));
        descriptor->next = next->next;
        if(next->next != nullptr) {
            next->next->previous = descriptor;
        }

    }

    // if the whole chunk is free, free the chunk
    AllocationDescriptor *firstChunkDescriptor = chunk->allocationListFirst;
    if(firstChunkDescriptor->type == AllocationType::Free && firstChunkDescriptor->size == fullPageAllocationSize) {

        // whole 2 MiB chunk is free, free it
        freeAndRemoveChunk(chunk);

    }

}

Heap::ChunkInfoBlock *Heap::allocateAndAppendNewChunk() {

    // firstly, allocate memory and adjust it for usage with paging
    usz address = reinterpret_cast<u64>(PhysicalAllocator::allocatePage(kernelPID, true));
    address += CPU::pagingBase;
    ChunkInfoBlock *newChunk = reinterpret_cast<ChunkInfoBlock*>(address);

    // create an allocation spanning whole page
    AllocationDescriptor *wholePageAllocation = reinterpret_cast<AllocationDescriptor*>(address + sizeof(ChunkInfoBlock));
    wholePageAllocation->next = nullptr;
    wholePageAllocation->previous = nullptr;
    wholePageAllocation->size = fullPageAllocationSize;
    wholePageAllocation->type = AllocationType::Free;
    newChunk->allocationListFirst = wholePageAllocation;

    // connect newly create chunk with others
    if(chunkListLength == 0) {
        newChunk->next = nullptr;
        newChunk->previous = nullptr;
        chunkListFirst = newChunk;
        chunkListLast = newChunk;
    }
    else {
        newChunk->next = nullptr;
        newChunk->previous = chunkListLast;
        chunkListLast->next = newChunk;
        chunkListLast = newChunk;
    }
    chunkListLength++;

    Logger::printFormat("[heap] new chunk for dynamic allocations created\n");

    return newChunk;

}

void Heap::freeAndRemoveChunk(ChunkInfoBlock *chunk) {

    // remove links to other chunks
    if(chunkListLength == 1) {
        chunkListFirst = nullptr;
        chunkListLast = nullptr;
    }
    else {

        if(chunk->previous != nullptr) chunk->previous->next = chunk->next;
        else chunkListFirst = chunk->next;

        if(chunk->next != nullptr) chunk->next->previous = chunk->previous;
        else chunkListLast = chunk->previous;
    }

    // decrease chunk count and free page
    PhysicalAllocator::freePage(reinterpret_cast<void*>(chunk));
    chunkListLength--;

    Logger::printFormat("[heap] existing chunk for dynamic allocations removed\n");

}

void *Heap::findAllocation(ChunkInfoBlock *chunk, usz size) {

    // implement first-fit algorithm of finding new allocation block
    AllocationDescriptor *current = chunk->allocationListFirst;
    while(current != nullptr) {

        // check if current fragment is suitable
        if(current->type == AllocationType::Free && current->size >= size) {

            // calculate size difference and check if split is even suitable
            usz sizeDifference = current->size - size;
            if(sizeDifference >= (sizeof(AllocationDescriptor) + allocationAlignment)) {

                // split alignment in two and return
                AllocationDescriptor *newDescriptor = reinterpret_cast<AllocationDescriptor*>(reinterpret_cast<usz>(current) + sizeof(AllocationDescriptor) + size);
                newDescriptor->size = current->size - sizeof(AllocationDescriptor) - size;
                newDescriptor->type = AllocationType::Free;
                newDescriptor->previous = current;
                newDescriptor->next = current->next;

                // change current descriptor
                current->size = size;
                current->next = newDescriptor;
                current->type = AllocationType::Allocated;         

            }

            // not suitable, so return whole allocation
            current->type = AllocationType::Allocated;
            return reinterpret_cast<void*>(reinterpret_cast<usz>(current) + sizeof(AllocationDescriptor));


        }

        // if current allocation is not suitable, 
        current = current->next;

    }

    // no suitable region found in current chunk, return nullptr
    return nullptr;

}

// new and delete operators
void *operator new(long unsigned int size) {
    return Heap::allocate(size);
}

void *operator new[](long unsigned int size) {
    return Heap::allocate(size);
}

void operator delete(void *address) {
    Heap::free(address);
}

void operator delete[](void *address) {
    Heap::free(address);
}

void operator delete(void *address, long unsigned int) {
    Heap::free(address);
}

void operator delete[](void *address, long unsigned int) {
    Heap::free(address);
}