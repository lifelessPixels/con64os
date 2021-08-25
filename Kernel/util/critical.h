#pragma once
#include <util/types.h>
#include <driver/arch/cpu.h>

/**
 * Class used to enter critical section in code
 */
class ScopedCritical {

public:
    ScopedCritical();
    ~ScopedCritical();

private:
    bool state = false;

};