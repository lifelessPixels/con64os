#pragma once
#include <mem/physalloc.h>
#include <util/types.h>

/**
 * @brief Class for managing GDT
 */
class GDT {

public:
    /**
     * @brief Creates GDT table and fills it accordingly
     */
    static void initialize();

    /**
     * @brief Sets segments of currently executing CPU to these contained in created GDT
     */
    static void switchKernelSegments();

    /**
     * @brief Gets number of segment to be written to CS register of CPU
     * @return Requested CS value
     */
    static u16 getKernelCodeSegment();

private:

    static void setGDTEntry(u8 entry, u8 dpl, u8 type, bool code = false) ;

    struct GDTEntry {
        u16 limitLow;
        u16 baseLow;
        u8 baseMedium;
        u8 type : 4;
        u8 system : 1;
        u8 dpl : 2;
        u8 present : 1;
        u8 limitHigh : 4;
        u8 available : 1;
        u8 longSegment: 1;
        u8 operationSize: 1;
        u8 granularity : 1;
        u8 baseHigh;
    } __attribute__((packed));

    static inline void *gdtPhysicalAddress = nullptr;
    static inline GDTEntry *gdt = nullptr;

};