#pragma once
#include <driver/arch/gdt.h>
#include <driver/arch/apic.h>
#include <util/types.h>
#include <util/logger.h>
#include <util/critical.h>
#include <mem/physalloc.h>

/**
 * @brief Class for managing interrupts of a system
 */
class Interrupts {

public:

    /**
     * @brief Initializes interrupts subsystem in the kernel
     */
    static void initialize();

    /**
     * @brief Loads currently executing CPU's IDTR register
     */
    static void loadIDT();

    /**
     * @brief Reserves a vector for Message Signaled Interrupts
     * @param handler Callback to be called when interrupt fires
     * @param handlerData Data to be passed to callback function
     * @return Reserved interrupt vector
     */
    static u8 reserveMSIVector(InterruptHandler handler, void *data);

    /**
     * @brief Reserves a vector for IOAPIC interrupts
     * @param handler Callback to be called when interrupt fires
     * @param handlerData Data to be passed to callback function
     * @return Reserved interrupt vector
     */
    static u8 reserveVector(InterruptHandler handler, void *data);

    /**
     * @brief Returns platform-specific address for MSI operation
     * @return Address at which MSI data should be written
     */
    static u64 getMSIAddress();

    /**
     * @brief Returns platform-specific data for MSI operation
     * @param vector Interrupt vector for MSI operation
     * @return Data to be written by MSI capable device
     */
    static u16 getMSIData(u8 vector);

    /**
     * @brief Entry point for firing interrupt handlers
     * @param Vector of interrupt to be fired
     */
    static void fireInterruptHandler(u8 vector);

private:

    static void setEntry(u8 entry, bool errorCode = false, void *handler = nullptr);

    struct IDTEntry {
        u16 offsetLow;
        u16 selector;
        u8 ist;
        u8 attributes;
        u16 offsetMedium;
        u32 offsetHigh;
        u32 zero;
    } __attribute__((packed));

    static inline void *idtPhysicalAddress = nullptr;
    static inline IDTEntry *idt = nullptr;
    static inline InterruptHandler *interruptHandlers = nullptr;
    static inline usz *interruptHandlersData = nullptr;
    static inline usz maxInterrupt = 0xfe;
    static inline usz minInterrupt = 0x20;

};
