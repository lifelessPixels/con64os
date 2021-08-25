#include <driver/acpi/acpibase.h>
#include <driver/ahci/ahcibase.h>
#include <driver/arch/cpu.h>
#include <driver/arch/apic.h>
#include <driver/arch/gdt.h>
#include <driver/arch/ints.h>
#include <driver/arch/hpet.h>
#include <driver/bus/pcie/pcie.h>
#include <driver/text/serial.h>
#include <driver/text/graphicsterm.h>
#include <mem/heap.h>
#include <mem/physalloc.h>
#include <mem/vas.h>
#include <util/bootboot.h>
#include <util/logger.h>
#include <util/spinlock.h>
#include <util/list.h>
#include <util/timer.h>

extern BootBoot::Structure bootboot;
extern u8 fb;

int kernelInitializationStage = 0;

extern "C"
void kernelMain() {

    // disable interrupts if they are for some reason enabled
    // CPU::setInterruptState(false);

    // activate all needed CPU extensions
    CPU::enableNXBit();
    CPU::enableSystemCallExtensions();

    // wait with other cores than BSP until main system parts are initialized
    if(CPU::getCoreAPICID() != bootboot.bspID) {

        // wait until BSP completed basic setup of kernel
        while(kernelInitializationStage == 0);

        // reload virtual address space
        CPU::writeCR3(reinterpret_cast<u64>(VirtualAddressSpace::getKernelVirtualAddressSpace()->getCR3()));

        // load GDT and IDT
        GDT::switchKernelSegments();
        Interrupts::loadIDT();

        // if kernel initialized enough, initialize APICs of other cores
        LAPIC::initializeCoreLAPIC();

        // enable interrupts on other cores
        // CPU::setInterruptState(true);

        // wait a bunch of time until scheduler is initialized
        while(kernelInitializationStage == 1);
        
    }

    // initialize kernel logger
    SerialPort loggerSerial(0x03f8);
    if(!loggerSerial.initializedCorrectly()) for(;;); // TODO: panic!
    Logger::setBackingDevice(&loggerSerial);
    Logger::printFormat("[main] con64OS is booting...\n");
    
    // print some crucial information
    Logger::printFormat("[main] bootstrap processor id: %u\n", bootboot.bspID);
    Logger::printFormat("[main] core count: %u\n", bootboot.coreCount);

    // switch to higher half entirely
    VirtualAddressSpace::adjustKernelMemory();

    // initialize bootboot global accessor
    BootBoot::registerStructure(&bootboot);

    // initialize physical allocator and kernel heap
    PhysicalAllocator::initialize();
    Heap::initialize();

    // create proper virtual address space for kernel
    new VirtualAddressSpace();

    // initialize ACPI subsystem
    ACPI::initialize();

    // initialize APIC subsystem and progress initialization stage
    APIC::initialize();
    LAPIC::initializeCoreLAPIC();

    // initialize essential CPU state
    GDT::initialize();
    GDT::switchKernelSegments();
    Interrupts::initialize();
    Interrupts::loadIDT();
    CPU::setInterruptState(true);

    // initialize HPET subsystem
    HPET::initialize();

    // initialize graphics terminal
    GraphicsTerminal *terminal = new GraphicsTerminal(reinterpret_cast<void*>(&fb), bootboot.framebufferWidth, bootboot.framebufferHeight, bootboot.framebufferScanline, bootboot.framebufferType);
    Logger::setBackingDevice(terminal);

    // initialize PCIe subsystem
    PCIe::initialize();

    // initialize AHCI subsystem
    AHCI::initialize();
    List<IBlockDevice*> *blockDevices = AHCI::getBlockDevices();
    for(usz i = 0; i < blockDevices->size(); i++) {
        IBlockDevice *device = blockDevices->get(i);
        Logger::printFormat("[main] found block device of size 0x%x sectors, writeable?: %b\n", device->sectorCount(), device->isWriteable());
    }

    // progress other cores
    Logger::printFormat("[main] progressing cores other than BSP...\n");
    kernelInitializationStage = 1;

    // show welcome message
    Logger::printFormat("[main] welcome to con64OS\n");
    Logger::printFormat("[main] kernel initialized successfully...\n");

    for(;;);
    return;

}