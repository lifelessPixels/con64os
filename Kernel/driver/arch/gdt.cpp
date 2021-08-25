#include "driver/arch/gdt.h"

CPU::Pointer pointer __attribute__((aligned(16)));

void GDT::initialize() {

    // create GDT
    gdtPhysicalAddress = PhysicalAllocator::allocatePage(kernelPID);
    void *gdtAddress = reinterpret_cast<void *>(reinterpret_cast<usz>(gdtPhysicalAddress) + CPU::pagingBase);
    gdt = reinterpret_cast<GDTEntry*>(gdtAddress);

    // zero-out GDT
    u64 *page = reinterpret_cast<u64*>(gdtAddress);
    for(usz i = 0; i < 512; i++) page[i] = 0ull;
    
    // fill GDT
    setGDTEntry(1, 0, 0x0a, true); // kernel code segment
    setGDTEntry(2, 0, 0x02); // kernel data segment
    setGDTEntry(3, 3, 0x0a, true); // user code segment
    setGDTEntry(4, 3, 0x02); // user data segment

    // fill the pointer
    pointer.size = 63;
    pointer.address = reinterpret_cast<u64>(gdt);

}

void GDT::switchKernelSegments() {

    // load gdt
    CPU::loadGDT(pointer);

    // load segments
    CPU::loadCodeSegment(0x08);
    CPU::loadDataSegments(0x10);

}

u16 GDT::getKernelCodeSegment() { return 0x08; }

void GDT::setGDTEntry(u8 entry, u8 dpl, u8 type, bool code) {

    gdt[entry].present = 1;
    gdt[entry].system = 1;

    gdt[entry].baseLow = 0x0000;
    gdt[entry].baseMedium = 0x00;
    gdt[entry].baseHigh = 0x00;

    gdt[entry].limitLow = 0xffff;
    gdt[entry].limitHigh = 0xf;

    gdt[entry].type = type & 0x0f;
    gdt[entry].dpl = dpl & 0b11;
    if(code) gdt[entry].longSegment = 1;
    else gdt[entry].operationSize = 1;
    gdt[entry].granularity = 1;

}