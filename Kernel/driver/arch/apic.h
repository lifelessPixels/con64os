#pragma once
#include <util/list.h>
#include <util/types.h>
#include <driver/acpi/acpibase.h>
#include <driver/arch/portio.h>
#include <driver/arch/ints.h>
#include <mem/vas.h>

/**
 * @brief Class for managing Local APIC of currently executing processor
 */
class LAPIC {

public:

    /**
     * @brief Initializes LAPIC local to currently executing CPU core
     */
    static void initializeCoreLAPIC();

    /**
     * @brief Returns vector number of currently processed interrupt
     * @return Number of interrupt vector (0-255)
     */
    static u8 getServicedInterruptVector();

    /**
     * @brief Sends EOI to LAPIC and IOAPIC after successfully servicing interrup
     */
    static void sendEOI();

private:

    static constexpr u32 lapicIDOffset = 0x020;
    static constexpr u32 lapicVersionOffset = 0x030;
    static constexpr u32 taskPriorityOffset = 0x080;
    static constexpr u32 arbitrationPriorityOffset = 0x090;
    static constexpr u32 processorPriorityOffset = 0x0a0;
    static constexpr u32 eoiOffset = 0x0b0;
    static constexpr u32 remoteReadOffset = 0x0c0;
    static constexpr u32 logicalDestinationOffset = 0x0d0;
    static constexpr u32 destinationFormatOffset = 0x0e0;
    static constexpr u32 spuriousInterruptVectorOffset = 0x0f0;
    static constexpr u32 inServiceOffset = 0x100;
    static constexpr u32 triggerModeOffset = 0x180;
    static constexpr u32 interruptRequestOffset = 0x200;
    static constexpr u32 errorStatusOffset = 0x280;
    static constexpr u32 correctedMachineCheckOffset = 0x2f0;
    static constexpr u32 interruptCommandOffset = 0x300;
    static constexpr u32 timerOffset = 0x320;
    static constexpr u32 thermalSensorOffset = 0x330;
    static constexpr u32 performanceMonitoringOffset = 0x340;
    static constexpr u32 lint0Offset = 0x350;
    static constexpr u32 lint1Offset = 0x360;
    static constexpr u32 errorOffset = 0x370;
    static constexpr u32 initialCountOffset = 0x380;
    static constexpr u32 currentCountOffset = 0x390;
    static constexpr u32 divideConfigurationOffset = 0x3e0;

    static u32 read(u32 offset);
    static void write(u32 offset, u32 value);

};

/**
 * @brief Class for managing IOAPIC
 */
class IOAPIC {

public:

    /**
     * @brief Initializes system IOAPIC
     * @param mmioObject Virtual address space object, where IOAPIC is mapped
     * @param mappedAddress Memory mapped address of IOAPIC registers
     * @param globalSystemInterruptBase First interrupt which is handled by this IOAPIC
     */
    static void initialize(MMIOVirtualMemoryObject *mmioObject, void *mappedAddress, u32 globalSystemInterruptBase);

    /**
     * @brief Tries to allocate IOAPIC input and sets redirection entry accordingly
     * @param entry IOAPIC pin on which the entry shopuld be set
     * @param handler Callback called on interrupt servicing
     * @param handlerData Data to be passed to callback function
     * @return true if registration was successful, false otherwise
     */
    static bool tryRegisterEntry(u8 entry, InterruptHandler handler, void *data);

private:

    static constexpr u32 ioapicIDIndex = 0x00;
    static constexpr u32 ioapicVersionIndex = 0x01;
    static constexpr u32 ioapicArbitrationIndex = 0x02;
    static constexpr u32 ioapicRedirectionBaseIndex = 0x10;

    static u32 read(u32 index);
    static void write(u32 index, u32 value);

    static inline MMIOVirtualMemoryObject *object;
    static inline volatile u32 *registers = nullptr;
    static inline u32 globalSystemInterrupt = 0;
    static inline usz redirectionEntryCount = 0;
    static inline bool initialized = false;
    static inline List<u8> *availablePins;

};

/**
 * @brief Class for managing APICs and IOAPICs in the system
 */

class APIC {

public:
    /**
     * @brief Gets MADT table and initializes state od APIC subsystem
     */
    static void initialize();

    /**
     * @brief Returns address of LAPIC in virtual memory
     * @return Address at which the LAPIC is mapped
     */
    static void *getLAPICAddress();

private:

    struct MADT : public ACPI::Table {
        u32 lapicAddress;
        u32 flags;
        u8 data[1];
    } __attribute__((packed));

    enum class MADTEntryType : u8 {
        LAPICEntry = 0,
        IOAPICEntry = 1,
        IOAPICSourceOverride = 2,
        IOAPICNMISource = 3,
        LAPICNMI = 4,
        LAPICAddressOverride = 5,
        LAPICx2Entry = 9
    };

    struct MADTEntry {
        MADTEntryType entryType;
        u8 length;
    } __attribute__((packed));

    struct LAPICEntryStruct : public MADTEntry {
        u8 acpiProcessorID;
        u8 apicID;
        u32 flags;
    }__attribute__((packed));

    struct IOAPICEntryStruct : public MADTEntry {
        u8 apicID;
        u8 reserved;
        u32 address;
        u32 globalSystemInterruptBase;
    } __attribute__((packed));

    struct IOAPICSourceOverrideStruct : public MADTEntry {
        u8 busSource;
        u8 irqSource;
        u32 globalSystemInterrupt;
        u16 flags;
    } __attribute__((packed));

    struct IOAPICNMISourceStruct : public MADTEntry {
        u8 nmiSource;
        u8 reserved;
        u16 flags;
        u32 globalSystemInterrupt;
    } __attribute__((packed));

    struct LAPICNMIStruct : public MADTEntry {
        u8 acpiProcessorID;
        u16 flags;
        u8 lintNumber;
    } __attribute__((packed));

    struct LAPICAddressOverrideStruct : public MADTEntry {
        u16 reserved;
        u64 lapicAddress;
    } __attribute__((packed));

    struct LAPICx2EntryStruct : public MADTEntry {
        u16 reserved;
        u32 apicID;
        u32 flags;
        u32 acpiID;
    } __attribute__((packed));

    static void disableLegacyPIC();

    static inline void *lapicAddress = nullptr;
    static inline void *mappedLAPICAddress = nullptr;

};

