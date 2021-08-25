#pragma once
#include <driver/arch/cpu.h>

/**
 * @brief Simple spinlock implementation
 */
class Spinlock {

public:
    Spinlock(const Spinlock & ) = delete;
    Spinlock(Spinlock && ) = delete;
    Spinlock() = default;
    void lock();
    bool isLocked();
    void unlock();

private:
    bool locked=  false;

    bool wereInterruptsEnabled=  false;

};
class ScopedSpinlock {

public:
    ScopedSpinlock() = delete;
    ScopedSpinlock(const ScopedSpinlock & ) = delete;
    ScopedSpinlock(ScopedSpinlock && ) = delete;

    ScopedSpinlock(Spinlock& lock);
    ~ScopedSpinlock();

private:
    Spinlock& spinlock;

};
