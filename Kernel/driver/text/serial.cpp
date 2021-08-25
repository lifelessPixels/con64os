
#include "serial.h"

SerialPort::SerialPort(u16 basePort, SerialPort::BaudRate baudRate, u8 settings) 
    : basePort(basePort), baudRate(baudRate), settings(settings) {

    // try to initialize serial port (assuming it is present)
    // disable interrupts generated by serial port
    PortIO::outputToPort8(this->basePort + OffsetInterruptEnableRegister, 0x00);

    // enable DLAB (Divisor Latch Access Bit) to change divisor
    PortIO::outputToPort8(this->basePort + OffsetLineControlRegister, 0x80);

    // set divisor
    PortIO::outputToPort8(this->basePort + OffsetDivisorLowRegister, static_cast<u8>(this->baudRate));
    PortIO::outputToPort8(this->basePort + OffsetDivisorHighRegister, 0x00);

    // set line mode and disable DLAB
    PortIO::outputToPort8(this->basePort + OffsetLineControlRegister, this->settings);

    // disable FIFO
    PortIO::outputToPort8(this->basePort + OffsetInterruptFIFORegister, 0x00);

    // enable loopback mode and check if serial port works by sending test value
    PortIO::outputToPort8(this->basePort + OffsetModemControlRegister, 0x1e);
    PortIO::outputToPort8(this->basePort + OffsetDataRegister, 0xfa);
    if(PortIO::inputFromPort8(this->basePort + OffsetDataRegister) != 0xfa) {
        this->initSuccessful = false;
        return;
    }

    // if port is working correctly, disable loopback mode
    PortIO::outputToPort8(this->basePort + OffsetModemControlRegister, 0x0f);

}

bool SerialPort::initializedCorrectly() {
    return this->initSuccessful;
}

void SerialPort::outputCharacter(u32 codepoint) {

    // busy wait for flag to be deasserted...
    while((PortIO::inputFromPort8(this->basePort + OffsetLineStatusRegister) & 0x20) == 0);

    // ...and send the data byte to output port (only 8 LSB from codepoint are sent)
    PortIO::outputToPort8(this->basePort + OffsetDataRegister, static_cast<u8>(codepoint));

}