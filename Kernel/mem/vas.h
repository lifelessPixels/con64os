#pragma once
#include <driver/arch/cpu.h>
#include <mem/physalloc.h>
#include <util/spinlock.h>
#include <util/types.h>
#include <util/list.h>

/**
 * Class encapsulating virtual memory object
 */
class VirtualMemoryObject {
    
public:

    static constexpr u8 writeable = (1 << 0);
    static constexpr u8 executable = (1 << 1);
    static constexpr u8 cacheable = (1 << 2);
    static constexpr u8 userMappable = (1 << 3);

    /**
     * @brief Constructor
     * @param accessParameters Access parameters of object
     * @param mappingAddress Address where object should be mapped
     */
    VirtualMemoryObject(u8 accessParameters, void *mappingAddress = nullptr);

    /**
     * @brief Destructor
     */
    ~VirtualMemoryObject();
    
    /**
     * @brief Returns object size
     * @return Object size in memory
     */

    usz objectSize();

    /**
     * @brief Returns whether object is large page aligned
     * @return true if object is large page aligned, false otherwise
     */
    bool largePageAligned();

    /**
     * @brief Returns objects address
     * @return Object physical address
     */
    void *objectAddress();

    /**
     * @brief Returns object flags
     * @return Object access parameters
     */
    u8 objectFlags();

    /**
     * @brief Returns how many pages the object is containing
     * @return Page count of object
     */
    List<void*> *objectPages();

protected:

    List<void*> *pages = nullptr;
    u8 flags = 0;
    usz size = 0;
    usz referenceCounter = 0;
    void *prefferedAddress = nullptr;
    Spinlock spinlock;
    bool largePageAlignmentNeeded = false;

};

/**
 * @brief Class encapsulating MMIO object
 */
class MMIOVirtualMemoryObject : public VirtualMemoryObject {

public:
    /**
     * @brief Constructor
     * @param physicalAddress Address of memory mapped region
     * @param length Length of region
     * @param mappingAddress Address where object should be mapped
     */
    MMIOVirtualMemoryObject(void *physicalAddress, usz length, void *mappingAddress = nullptr);

};

/**
 * @brief Class encapsulating memory object
 */
class MemoryBackedVirtualMemoryObject : public VirtualMemoryObject {

public:
    /**
     * @brief Constructor
     * @param length Length of region
     * @param disallowLargePages Whether large pages are allowed
     * @param mappingAddress Address where object should be mapped
     * @param write Whether region should be writeable
     * @param execute Whethter region should be executable
     * @param cache Whethter region should be cacheable
     * @param pid PID of process
     */
    MemoryBackedVirtualMemoryObject(usz length, bool disallowLargePages = false, void *mappingAddress = nullptr, bool write = false, bool execute = false, bool cache = true, u32 pid = kernelPID);

    /**
     * @brief Destructor - frees allocated pages
     */
    ~MemoryBackedVirtualMemoryObject();

};

/**
 * @brief Class encapsulating single page object
 */
class UncacheablePageVirtualMemoryObject : public VirtualMemoryObject {

public:
    /**
     * @brief Constructor
     * @param large Whether large page is used
     * @param mappingAddress Address where object should be mapped
     */
    UncacheablePageVirtualMemoryObject(bool large = false, void *mappingAddress = nullptr);
    /**
     * @brief Destructor - frees allocated page
     */
    ~UncacheablePageVirtualMemoryObject();

    void *getPhysicalAddress();
    
};

/**
 * @brief Class for servicing single virtual address space
 */
class VirtualAddressSpace {

public:

    /**
     * @brief Constructor
     */
    VirtualAddressSpace();

    /**
     * @brief Maps object into address space
     * @param object Object to be mapped
     * @return Virtual address of mapped object
     */
    void *mapObject(VirtualMemoryObject *object);

    /**
     * @brief Returns physical address of mapping structure
     * @return Physical address of mapping structure
     */
    void *getCR3();

    /**
     * @brief Adjusts kernel memory map to be fully higher half
     */
    static void adjustKernelMemory();

    /**
     * @brief Returns kernel address space
     * @return Object of kernel address space
     */
    static VirtualAddressSpace *getKernelVirtualAddressSpace();

private:

    union PML4Entry {

        struct {
            u64 present : 1;
            u64 writeEnable : 1;
            u64 userAccessible : 1;
            u64 writeThrough : 1;
            u64 cacheDisable : 1;
            u64 accessed : 1;
            u64 reserved : 6;
            u64 address : 40;
            u64 ignored : 11;
            u64 executionDisable: 1;
        };

        u64 value;

    } __attribute__((packed));

    union PDPTEntry {

        struct {
            u64 present : 1;
            u64 writeEnable : 1;
            u64 userAccessible : 1;
            u64 writeThrough : 1;
            u64 cacheDisable : 1;
            u64 accessed : 1;
            u64 reserved : 6;
            u64 address : 40;
            u64 ignored : 11;
            u64 executionDisable: 1;
        };

        u64 value;

    } __attribute__((packed));

    union PDEntry {

        struct {
            u64 present : 1;
            u64 writeEnable : 1;
            u64 userAccessible : 1;
            u64 writeThrough : 1;
            u64 cacheDisable : 1;
            u64 accessed : 1;
            u64 reserved : 6;
            u64 address : 40;
            u64 ignored : 11;
            u64 executionDisable: 1;
        } ptReference;

        struct {
            u64 present : 1;
            u64 writeEnable : 1;
            u64 userAccessible : 1;
            u64 writeThrough : 1;
            u64 cacheDisable : 1;
            u64 accessed : 1;
            u64 dirty : 1;
            u64 pageSize : 1;
            u64 global : 1;
            u64 reserved : 12;
            u64 address : 31;
            u64 ignored : 11;
            u64 executionDisable: 1;
        } largePageReference;

        u64 value;

    } __attribute__((packed));

    union PTEntry {

        struct {
            u64 present : 1;
            u64 writeEnable : 1;
            u64 userAccessible : 1;
            u64 writeThrough : 1;
            u64 cacheDisable : 1;
            u64 accessed : 1;
            u64 dirty : 1;
            u64 reserved1 : 1;
            u64 global : 1;
            u64 reserved : 3;
            u64 address : 40;
            u64 ignored : 11;
            u64 executionDisable: 1;
        };

        u64 value;

    } __attribute__((packed));

    struct VirtualMemoryRegion {

        enum class Type {
            Free,
            Allocated
        };

        Type type;
        VirtualMemoryObject *object = nullptr;
        usz size;
        usz address;

    };

    static inline VirtualAddressSpace *kernelAddressSpace = nullptr;
    static inline bool kernelAddressSpaceInitialized = false;
    static void *allocateZeroedPage();

    void *getMappingEntry(void *address, bool large = false, bool create = false);
    void doMapping(VirtualMemoryRegion *region);

    void *cr3Value = nullptr;
    PML4Entry *mappingStructure = nullptr;
    List<VirtualMemoryRegion*> *allocationList = nullptr;
    Spinlock spinlock;

};