#include "util/timer.h"


Timer::Timer() : fired(false), running(false) {}

void Timer::wait(usz milliseconds) {

    // don't do anything if timer is already running
    if(running) return;

    // set fields accordingly
    fired = false;
    running = true;

    // set timer in HPET
    eventID = HPET::createTimedEvent(milliseconds, &Timer::eventHandler, reinterpret_cast<void*>(this));

    // wait until interrupt fired
    while(!fired);

    // set fields
    fired = false;
    running = false;

}

void Timer::nonBlockingWait(usz milliseconds) {

    // don't do anything if timer is already running
    if(running) return;

    // set fields accordingly
    fired = false;
    running = true;

    // set timer in HPET
    eventID = HPET::createTimedEvent(milliseconds, &Timer::eventHandler, reinterpret_cast<void*>(this));

}

void Timer::disableNonBlockingWait() {

    // set field accordingly
    running = false;
    fired = false;
    HPET::removeTimedEvent(eventID);

}

bool Timer::wasFired() { return fired; }

void Timer::eventHandler(void *timer) {

    // convert pointer
    Timer *castedTimer = reinterpret_cast<Timer*>(timer);

    // make fired if running
    if(!castedTimer->running) return;
    castedTimer->fired = true;
    castedTimer->running = false;

}
