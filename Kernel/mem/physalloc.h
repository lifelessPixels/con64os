#pragma once
#include <util/bootboot.h>
#include <util/logger.h>
#include <util/spinlock.h>
#include <util/types.h>

/**
 * @brief Class for managing physical memory allocations
 */

class PhysicalAllocator {
public:

    static constexpr u32 pageSize = 4096;
    static constexpr u32 largePageSize = 2 * 1024 * 1024;

    /**
     * @brief Initializes physical memory allocator
     */
    static void initialize();

    /**
     * @brief Allocates page
     * @param pid PID of process for which the page is allocated
     * @param large Specifies whether page should be 2MiB (true) or 4KiB (false)
     * @return Address of allocated page
     */
    static void *allocatePage(u32 pid, bool large = false);

    /**
     * @brief Frees allocated page
     * @param address Address of page to be freed
     */
    static void freePage(void * address);

private:

    // only allow allocation of first 16GiB of physical memory
    static constexpr u64 briefBitmapPages = 1;

    // for each 2GiB of physical memory, 1 page of description is needed
    static constexpr u64 largePageBitmapPages = briefBitmapPages * 16;
    static constexpr u64 maxLargePage = largePageBitmapPages * 1024 / 2;
    static constexpr u32 reservedProcessID = 0xffffff;

    enum class BriefBitmapEntryType : u8 {
		FullyFree = 0b00,
		FullySingleAllocated = 0b01,
		PartiallyFree = 0b10,
		FullyPageAllocated = 0b11 // this 2MiB region is fully allocated (single pages in this region are allocated using allocation bitmap)
    };

    enum class AllocationBitmapEntryFlags : u8 {
		Allocated = 0x01,
		Reserved = 0x02
    };

    struct AllocationBitmapEntry {
        u32 ProcessID : 24;
        u32 Flags : 8;
    };
    
    struct LargePageAllocationBitmap {
        u32 FreePages;
        AllocationBitmapEntry AllocationEntries[511];
    };

    // pointers to bitmaps
    static inline u8 *briefBitmap = nullptr;
    static inline AllocationBitmapEntry *largePageBitmap = nullptr;
    static inline u64 freePagesCount = 0;
    static inline u64 freeLargePagesCount = 0;

    // spinlock to ensure mutual exclusion
    static inline Spinlock allocatorSpinlock;

    static void setBriefBitmapEntry(u64 pageIndex, BriefBitmapEntryType type);
    static void setLargePageBitmapEntry(u64 pageIndex, u32 pid, u8 flags);
    static BriefBitmapEntryType getBriefBitmapEntry(u64 pageIndex);
    static AllocationBitmapEntry getLargePageBitmapEntry(u64 pageIndex);

};

