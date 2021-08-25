#include "util/critical.h"

ScopedCritical::ScopedCritical() {
    state = CPU::enterCritical();
}

ScopedCritical::~ScopedCritical() {
    CPU::exitCritical(state);
}