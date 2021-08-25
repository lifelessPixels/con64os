#pragma once
#include <util/types.h>
#include <driver/acpi/acpibase.h>
#include <driver/arch/ints.h>
#include <mem/vas.h>

/**
 * @brief Class for managin High Precision Event Timer
 */
class HPET {

public:

    /**
     * @brief Gets specific ACPI table and initializes HPET 
     */
    static void initialize();

    /**
     * @brief Creates timed event to be raised in specific period of time
     * @param milliseconds How many milliseconds from now should event be fired
     * @param handler Callback to be called after event is fired
     * @param handlerData Data to be passed to callback function
     * @return ID of creted event
     */
    static usz createTimedEvent(u64 milliseconds, EventHandler handler, void *handlerData);

    /** 
     * @brief Removes created timed event
     * @param id ID of event to be removed
     */
    static void removeTimedEvent(usz id);

private:

    struct TimerConfiguration {
        u64 configurationAndCapabilities;
        u64 comparatorValue;
        u64 fsbInterruptRoute;
        u64 padding;
    };

    struct RegisterSpace {
        u64 generalCapabilities;
        u64 reserved1;
        u64 generalConfiguration;
        u64 reserved2;
        u64 generalInterruptStatus;
        u64 reserved3;
        u64 reserved4[24];
        u64 mainCounterValue;
        u64 reserved5;
        volatile TimerConfiguration timers[1];
    };

    struct HPETTable : ACPI::Table {
        u32 eventTimerBlockID;
        ACPI::GenericAddressStructure baseAddress;
        u8 hpetNumber;
        u16 mainCounterMinimumPeriodic;
        u8 pageProtection;
    } __attribute__((packed));

    struct TimedEvent {
        u64 tickCount;
        EventHandler handler;
        void *handlerData;
        usz id = 0;
    };

    static constexpr usz femtosecondPerMillisecond = 1000000000000ull;

    static void setupOneShotMillisecond();
    static void oneShotInterruptHandler(void *, u32);

    static inline volatile RegisterSpace *registers = nullptr;
    static inline u32 clockPeriod = 0;
    static inline u8 numberOfTimers = 0;
    static inline List<TimedEvent*> *eventQueue = nullptr;
    static inline Spinlock eventQueueSpinlock;
    static inline usz currentTickCount = 0;
    static inline u8 oneShotTimer = 0xff;
    static inline u8 oneShotRouting = 0xff;
    static inline u8 periodicTimer = 0xff;
    static inline usz currentID = 1;

};