#include "driver/arch/hpet.h"

void HPET::initialize() {

    // get HPET table
    void *hpet = ACPI::getTableBySignature("HPET");
    if(!hpet) {
        Logger::printFormat("[hpet] HPET table not found, aborting...\n");
        for(;;); // TODO: panic!
    }
    HPETTable *table = reinterpret_cast<HPETTable*>(hpet);

    u64 address = table->baseAddress.address;
    Logger::printFormat("[hpet] found HPET at address 0x%x\n", address);

    // create mapping for found hpet
    MMIOVirtualMemoryObject *hpetObject = new MMIOVirtualMemoryObject(reinterpret_cast<void*>(address), 4096);
    registers = reinterpret_cast<RegisterSpace*>(VirtualAddressSpace::getKernelVirtualAddressSpace()->mapObject(hpetObject));
    if(!registers) {
        Logger::printFormat("[hpet] could not map HPET, aborting...\n");
        for(;;); // TODO: panic!
    }

    // get some info
    clockPeriod = (registers->generalCapabilities >> 32) & 0xffffffff;
    numberOfTimers = ((registers->generalCapabilities >> 8) & 0b11111) + 1;
    Logger::printFormat("[hpet] timers: %u, clock period: 0x%x femtoseconds\n", numberOfTimers, clockPeriod);

    // get some info about all timers
    bool periodicSupported = false;
    periodicTimer = 0xff;
    oneShotTimer = 0xff;
    for(usz i = 0; i < numberOfTimers; i++) {
        u64 capabilities = registers->timers[i].configurationAndCapabilities;
        u32 routing = capabilities >> 32;
        bool periodic = capabilities & (1 << 4);
        bool fsbRouting = capabilities & (1 << 15);
        Logger::printFormat("[hpet]   - timer %d - capabilites: 0x%x, routing: 0x%x, periodic?: %b, fsb?: %b\n", i, capabilities, routing, periodic, fsbRouting);

        if(!periodicSupported && periodic) {
            periodicSupported = true;
            periodicTimer = i;
        }
        else if(oneShotTimer == 0xff) {
            oneShotTimer = i;
        }
    }

    // create event queue
    eventQueue = new List<TimedEvent*>();

    // check if any timer supports periodic mode
    if(!periodicSupported) {
        Logger::printFormat("[hpet] HPET does not support periodic mode at any timer, aborting...\n");
        for(;;); // TODO: panic!
    }
    else Logger::printFormat("[hpet] timer %d will be used in periodic mode\n", periodicTimer);
    if(oneShotTimer == 0xff) {
        Logger::printFormat("[hpet] HPET does not have any timer to be used in one shot timer, aborting...\n");
        for(;;); // TODO: panic!
    }
    else Logger::printFormat("[hpet] timer %d will be used in one shot mode\n", oneShotTimer);

    // disable legacy replacement
    registers->generalConfiguration = registers->generalConfiguration & ~(1 << 1);

    // register handler and IOAPIC input for one shot timer
    u64 capabilities = registers->timers[oneShotTimer].configurationAndCapabilities;
    u32 routing = capabilities >> 32;
    oneShotRouting = 0;
    bool success = false;
    for(usz i = 0; i < 32; i++) {
        if(routing & (1 << i)) {
            success = IOAPIC::tryRegisterEntry(i, &HPET::oneShotInterruptHandler, nullptr);
            if(success) {
                oneShotRouting = i;
                break;
            }
        }
    }

    if(!success) {
        Logger::printFormat("[hpet] cannot assign IOAPIC input for one shot timer, aborting...\n");
        for(;;); // TODO: panic!
    }

    // configure one shot timer for the first time
    setupOneShotMillisecond();
    Logger::printFormat("[hpet] one shot timer initialized\n");

}

usz HPET::createTimedEvent(u64 milliseconds, EventHandler handler, void *handlerData) {

    // check if time is at least 1 ms
    if(milliseconds == 0) return 0;

    // lock spinlock
    ScopedSpinlock lock(eventQueueSpinlock);

    // decrement all timed events by current count and zero it
    for(usz i = 0; i < eventQueue->size(); i++) {
        TimedEvent *event = eventQueue->get(i);
        event->tickCount -= currentTickCount;
    }
    currentTickCount = 0;

    // iterate and check where insertion is needed
    usz position = 0;
    bool inTheMiddle = false;
    for(usz i = 0; i < eventQueue->size(); i++) {
        TimedEvent *event = eventQueue->get(i);
        if(milliseconds <= event->tickCount) {
            inTheMiddle = true;
            break;
        }
        position = i;
    }
    if(!inTheMiddle) position++;

    // create new event
    usz id = currentID++;
    TimedEvent *event = new TimedEvent();
    event->handler = handler;
    event->handlerData = handlerData;
    event->tickCount = milliseconds;
    event->id = id;

    // insert event
    eventQueue->insertAt(event, position);

    // return id
    return id;

}

void HPET::removeTimedEvent(usz id) {

    // lock spinlock
    ScopedSpinlock lock(eventQueueSpinlock);

    // iterate and remove if still in the list
    for(usz i = 0; i < eventQueue->size(); i++) {
        TimedEvent *event = eventQueue->get(i);
        if(event->id == id) {
            eventQueue->remove(i);
            break;
        }
    }

}

void HPET::setupOneShotMillisecond() {

    // disable main counter
    registers->generalConfiguration = registers->generalConfiguration & ~(1 << 0);

    // setup timer
    u64 time = femtosecondPerMillisecond / clockPeriod;
    u64 wantedTime = registers->mainCounterValue + time;
    registers->timers[oneShotTimer].configurationAndCapabilities = ((oneShotRouting & 0b11111) << 9) | (1 << 2); // enable interrupts
    registers->timers[oneShotTimer].comparatorValue = wantedTime;
    registers->generalInterruptStatus = 0xffffffffffffffff;

    // enable main counter
    registers->generalConfiguration = registers->generalConfiguration | (1 << 0);

}

void HPET::oneShotInterruptHandler(void *, u32) {

    // check if any events are pending
    if(eventQueue->size() == 0) {
        setupOneShotMillisecond();
        currentTickCount = 0;
        return;
    }

    // if there are, increment counter
    currentTickCount++;

    // lock the spinlock
    ScopedSpinlock lock(eventQueueSpinlock);

    // compare current tick count
    TimedEvent *firstEvent = eventQueue->get(0);
    if(currentTickCount >= firstEvent->tickCount) {

        // subtract current count from all queued events
        for(usz i = 0; i < eventQueue->size(); i++) {
            TimedEvent *event = eventQueue->get(i);
            event->tickCount -= (currentTickCount > event->tickCount) ? event->tickCount : currentTickCount;
        }

        // fire all events that have count 0
        TimedEvent *currentEvent = eventQueue->get(0);
        while(currentEvent->tickCount == 0) {
            
            // fire event
            currentEvent->handler(currentEvent->handlerData);

            // delete event object and remove it from the list
            delete currentEvent;
            eventQueue->remove(0);

            if(eventQueue->size() == 0) break;
            else currentEvent = eventQueue->get(0);
        }

        // zero-out current tick count
        currentTickCount = 0;

    }

    // reset the one shot timer to one millisecond from now
    setupOneShotMillisecond();

}