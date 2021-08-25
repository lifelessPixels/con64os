#include "util/spinlock.h"

void Spinlock::lock() {
    
    // enter critical section
    wereInterruptsEnabled = CPU::enterCritical();

    // try to lock indefinately
    bool expected = false;
    while(!__atomic_compare_exchange_n(&locked, &expected, true, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) { expected = false; }

}

bool Spinlock::isLocked() {

    // return atomically the value of spinlock
    return __atomic_load_n(&locked, __ATOMIC_ACQUIRE);

}

void Spinlock::unlock() {

    // check if locked
    if(!isLocked()) return;

    // store "false" value in spinlock
    __atomic_store_n(&locked, false, __ATOMIC_RELEASE);

    // exit critical section
    CPU::exitCritical(wereInterruptsEnabled);

}

ScopedSpinlock::ScopedSpinlock(Spinlock & lock) : spinlock(lock) {
    spinlock.lock();
}

ScopedSpinlock::~ScopedSpinlock() {
    spinlock.unlock();
}

