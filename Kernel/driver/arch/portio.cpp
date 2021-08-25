
#include "portio.h"

void PortIO::outputToPort8(u16 port, u8 data) {

    // use inline assembly to send data to port
    asm volatile ("outb %0, %1" : : "a"(data), "Nd"(port));

}

void PortIO::outputToPort16(u16 port, u16 data) {

    // use inline assembly to send data to port
    asm volatile ("outw %0, %1" : : "a"(data), "Nd"(port));

}

void PortIO::outputToPort32(u16 port, u32 data) {

    // use inline assembly to send data to port
    asm volatile ("outl %0, %1" : : "a"(data), "Nd"(port));

}

u8 PortIO::inputFromPort8(u16 port) {

    // use inline assembly to send data to port
    u8 data;
    asm volatile ("inb %1, %0" : "=a"(data) : "Nd"(port));
    return data;

}

u16 PortIO::inputFromPort16(u16 port) {

    // use inline assembly to send data to port
    u16 data;
    asm volatile ("inw %1, %0" : "=a"(data) : "Nd"(port));
    return data;

}

u32 PortIO::inputFromPort32(u16 port) {

    // use inline assembly to send data to port
    u32 data;
    asm volatile ("inl %1, %0" : "=a"(data) : "Nd"(port));
    return data;

}

