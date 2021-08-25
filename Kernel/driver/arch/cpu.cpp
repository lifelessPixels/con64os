
#include "cpu.h"


CPU::CPUID CPU::getCPUID(u32 leaf) {
    
    CPUID cpuid;
    cpuid.leaf = leaf;
    __get_cpuid(leaf, &cpuid.aRegister, &cpuid.bRegister, &cpuid.cRegister, &cpuid.dRegister);
    return cpuid;

}

u8 CPU::getCoreAPICID() {

    CPUID cpuid = getCPUID(1);
    return static_cast<u8>(cpuid.bRegister >> 24);

}

void CPU::setInterruptState(bool interruptState) {

    if(interruptState) asm volatile ("sti");
    else asm volatile ("cli");

}

bool CPU::getInterruptState() {

    return ((readEFLAGS() & (1 << 9)) > 0);

}

bool CPU::enterCritical() {

    bool returnState = getInterruptState();
    setInterruptState(false);
    return returnState;

}

void CPU::exitCritical(bool interruptState) {

    setInterruptState(interruptState);

}

void CPU::loadGDT(Pointer pointer) {

    asm volatile("lgdt %0" : : "m"(pointer));

}

void CPU::loadIDT(Pointer pointer) {

    asm volatile("lidt %0" : : "m"(pointer));

}

void CPU::loadCodeSegment(u16 segment) {

    (void)(segment);
    asm volatile("pushq $0x08\n"
                 "pushq $.reloadSegment \n"
                 "retfq \n"
                 ".reloadSegment: \n"
                 "nop \n");

}

void CPU::loadDataSegments(u16 segment) {

    asm volatile("mov %0, %%ds" : : "a"(segment));
    asm volatile("mov %0, %%ss" : : "a"(segment));
    asm volatile("mov %0, %%es" : : "a"(segment));
    asm volatile("mov %0, %%fs" : : "a"(segment));
    asm volatile("mov %0, %%gs" : : "a"(segment));

}

u64 CPU::readEFLAGS() {

    u64 flags;
    asm volatile("pushf ; pop %0" : "=rm"(flags) : : "memory");
    return flags;

}

u64 CPU::readCR3() {

    u64 value;
    asm volatile ("mov %%cr3, %0" : "=r"(value));
    return value;

}

void CPU::writeCR3(u64 value) {

    asm volatile ("mov %0, %%cr3" : : "r"(value));
    
}

void CPU::invalidatePagingEntry(void *address) {

    asm volatile ("invlpg (%0)" : : "b"(address) : "memory");

}

u64 CPU::readMSR(u32 msr) {

    u32 lower, higher;
    asm volatile ("rdmsr" : "=a"(lower), "=d"(higher) : "c"(msr));
    return (static_cast<u64>(higher) << 32) | lower;

}

void CPU::writeMSR(u32 msr, u64 value) {

    u32 lower = value & 0xffffffff;
    u32 higher = value >> 32;
    asm volatile("wrmsr" : : "c"(msr), "a"(lower), "d"(higher));

}

void CPU::enableNXBit() {

    writeMSR(eferMSRAddress, readMSR(eferMSRAddress) | (1ull << 11));

}

void CPU::enableSystemCallExtensions() {

    writeMSR(eferMSRAddress, readMSR(eferMSRAddress) | (1ull << 0));

}


