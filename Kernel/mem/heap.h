#pragma once
#include <driver/arch/cpu.h>
#include <mem/physalloc.h>
#include <util/types.h>
#include <util/spinlock.h>

/**
 * Class for servicing heap of the kernel
 */
class Heap {
public:

	/**
	 * @brief Initializes kernel heap
	 */
	static void initialize();

	/**
	 * @brief Allocates chunk of memory for kernel use
	 * @param size Size of requested chunk
	 * @return Address of the allocated chunk
	 */
	static void *allocate(usz size);

	/**
	 * @brief Frees previously allocated chunk
	 * @param address Address of the chunk to be freed
	 */
	static void free(void *address);	

private:

	enum class AllocationType : usz {
		Free,
		Allocated
	};

	struct AllocationDescriptor {
		AllocationDescriptor *previous;
		AllocationDescriptor *next;
		AllocationType type;
		usz size;
	} __attribute__((packed));
	
	struct ChunkInfoBlock {
		ChunkInfoBlock *previous;
		ChunkInfoBlock *next;
		AllocationDescriptor *allocationListFirst;
		usz reservedAlignment;
	} __attribute__((packed)); 

	static constexpr usz fullPageAllocationSize = PhysicalAllocator::largePageSize - sizeof(ChunkInfoBlock) - sizeof(AllocationDescriptor);
	static constexpr usz allocationAlignment = sizeof(AllocationDescriptor);

	static inline ChunkInfoBlock *chunkListFirst = nullptr;
	static inline ChunkInfoBlock *chunkListLast = nullptr;
	static inline usz chunkListLength = 0;
	static inline Spinlock heapSpinlock;

	static ChunkInfoBlock *allocateAndAppendNewChunk();
	static void freeAndRemoveChunk(ChunkInfoBlock *chunk);
	static void *findAllocation(ChunkInfoBlock *chunk, usz size);

};

