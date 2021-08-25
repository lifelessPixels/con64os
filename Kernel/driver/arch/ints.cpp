#include "driver/arch/ints.h"

CPU::Pointer __attribute__((aligned(16))) idtPointer;

struct InterruptFrame {
    u64 values[1];
};

static usz spuriousInterruptCount = 0;

__attribute__((interrupt))
static void interruptHandler(InterruptFrame *) {

    // check for spurious interrupt
    u8 vector = LAPIC::getServicedInterruptVector();
    if(vector != 0) {
        
        // service interrupt
        // if(vector != 0x20) Logger::printFormat("interrupt no. 0x%x received\n", vector);

        // fire handler
        Interrupts::fireInterruptHandler(vector);

        // send EOI
        LAPIC::sendEOI();

    }

    else {

        // spurious interrupt happened, ignoring...
        spuriousInterruptCount++;
        Logger::printFormat("[ints] spurious interrupt happened, %u so far\n", spuriousInterruptCount);

    }

}

__attribute__((interrupt))
static void exceptionHandler(InterruptFrame *, unsigned long int) {

    // print some gibberish
    Logger::printFormat("exception no. 0x%x received\n", LAPIC::getServicedInterruptVector());

}

__attribute__((interrupt))
static void generalProtectionFault(InterruptFrame *frame, unsigned long int code) {

    // print some data
    Logger::printFormat("[ints] general protection fault, code: 0x%x, frame[0] = 0x%x, frame[1] = 0x%x\n", static_cast<u64>(code), frame->values[0], frame->values[1]);
    for(;;);

}

void Interrupts::initialize() {

    // create IDT
    idtPhysicalAddress = PhysicalAllocator::allocatePage(kernelPID);
    void *idtAddress = reinterpret_cast<IDTEntry*>(reinterpret_cast<usz>(idtPhysicalAddress) + CPU::pagingBase);
    idt = reinterpret_cast<IDTEntry*>(idtAddress);

    // create interrupt handlers tables
    interruptHandlers = new InterruptHandler[256];
    for(usz i = 0; i < 256; i++) interruptHandlers[i] = nullptr;
    interruptHandlersData = new usz[256];
    for(usz i = 0; i < 256; i++) interruptHandlersData[i] = 0;

    // zero-out IDT
    u64 *page = reinterpret_cast<u64*>(idtAddress);
    for(usz i = 0; i < 512; i++) page[i] = 0ull;

    // fill processor exceptions and spurious interrupt vector
    // TODO: do it
    // if(i == 8 || i == 10 || i == 11 || i == 12 || i == 13 || i == 14 || i == 17 || i == 30) setEntry(i, true);

    setEntry(13, true, reinterpret_cast<void*>(&generalProtectionFault));

    // fill other IDT entries (everything other than, first 32 vectors)
    for(usz i = 32; i < 256; i++) { setEntry(i); }

    // fill the pointer
    idtPointer.size = 4095;
    idtPointer.address = reinterpret_cast<u64>(idtAddress);

}

void Interrupts::loadIDT() {

    // just call CPU function
    CPU::loadIDT(idtPointer);

}

u8 Interrupts::reserveMSIVector(InterruptHandler handler, void *data) {

    // check if vectors are available
    if(maxInterrupt - minInterrupt == 0) return 0; 

    // register handler for that
    interruptHandlers[minInterrupt] = handler;
    interruptHandlersData[minInterrupt] = reinterpret_cast<usz>(data);

    // return vectors
    u8 vector = minInterrupt;
    minInterrupt += 1;
    return vector;

}

u8 Interrupts::reserveVector(InterruptHandler handler, void *data) {

    // same functionality as MSI reserve
    return reserveMSIVector(handler, data);

}

u64 Interrupts::getMSIAddress() {

    // return address as specified in Intel manuals
    // NOTE: all MSI will be redirected to bootstrap processor for simplicity

    u64 address = 0xfee00000ull;
    u8 bspLAPICID = BootBoot::getStructure().bspID;
    address |= (bspLAPICID << 12);

    // return address
    return address;

}

u16 Interrupts::getMSIData(u8 vector) {

    // prepare data for specific vector
    // NOTE: using trigger mode "edge"
    u16 data = static_cast<u16>(vector);

    // return data
    return data;

}

void Interrupts::fireInterruptHandler(u8 vector) {

    // just call the function with according data
    interruptHandlers[vector](reinterpret_cast<void*>(interruptHandlersData[vector]), 0);

}

void Interrupts::setEntry(u8 entry, bool errorCode, void *handler) {

    // get address of routine
    u64 routineAddress = (errorCode) ? reinterpret_cast<u64>(&exceptionHandler) : reinterpret_cast<u64>(&interruptHandler);
    if(handler) routineAddress = reinterpret_cast<u64>(handler);

    // fill the idt entry
    idt[entry].selector = GDT::getKernelCodeSegment();
    idt[entry].offsetLow = routineAddress & 0xffff;
    idt[entry].offsetMedium = (routineAddress >> 16) & 0xffff;
    idt[entry].offsetHigh = (routineAddress >> 32) & 0xffffffff;
    idt[entry].attributes = 0x0e | (1 << 7);

}
