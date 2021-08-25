#pragma once
#include <util/types.h>
#include <driver/arch/hpet.h>

/**
 * @brief Simple timer implementation - using HPET
 */
class Timer {

public:
    Timer();
    void wait(usz milliseconds);
    void nonBlockingWait(usz milliseconds);
    void disableNonBlockingWait();
    bool wasFired();

private:
    static void eventHandler(void *timer);
    bool fired = false;
    bool running = false;
    usz eventID = 0;

};