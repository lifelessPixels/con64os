#include "util/logger.h"

void Logger::setBackingDevice(ITextOutput *device) {
    currentOutputDevice = device;
}

void Logger::printFormat(const char *format) {

    // lock spinlock if needed
    if(printRecursionDepth == 0) loggerSpinlock.lock();

    // make text colorful (green), print format and reset color attribute
    print(format);

    // unlock spinlock 
    if(printRecursionDepth == 0) loggerSpinlock.unlock();

}

void Logger::print(const char character) {

    // if for some reason, we don't have backing device, just return
    if(currentOutputDevice == nullptr) return;

    // print the character
    currentOutputDevice->outputCharacter(static_cast<u32>(character));

}

void Logger::print(const char *string) {

    // if for some reason, we don't have backing device, just return
    if(currentOutputDevice == nullptr) return;

    // iterate through string and print letter by letter
    while(*string != '\0') currentOutputDevice->outputCharacter(static_cast<u32>(*string++));

}

void Logger::print(const u32 value) {

    // if for some reason, we don't have backing device, just return
    if(currentOutputDevice == nullptr) return;

    // if number is zero, just print 0
    if(value == 0) {
        print('0');
        return;
    }

    // prepare useful variables
    const char *translation = (shouldNextNumberBeHex ? hexadecimalTranslation : decimalTranslation);
    const u32 base = (shouldNextNumberBeHex ? 16 : 10);
    char converted[16];
    for(u32 i = 0; i < 16; i++) converted[i] = '0';
    converted[15] = '\0';
    char *toInsert = &converted[14];
    u32 valueCopy = value;

    // do conversion
    while(valueCopy != 0) {
        *toInsert = translation[valueCopy % base];
        valueCopy /= base;
        toInsert--;
    }

    // print value
    if(shouldNextNumberBeHex) print(&converted[7]);
    else print(toInsert + 1);

    // if we printed hex, disable this state
    shouldNextNumberBeHex = false;

}

void Logger::print(const u64 value) {

    // if for some reason, we don't have backing device, just return
    if(currentOutputDevice == nullptr) return;

    // if number is zero, just print 0
    if(value == 0) {
        print('0');
        return;
    }

    // prepare useful variables
    const char *translation = (shouldNextNumberBeHex ? hexadecimalTranslation : decimalTranslation);
    const u32 base = (shouldNextNumberBeHex ? 16 : 10);
    char converted[32];
    for(u32 i = 0; i < 32; i++) converted[i] = '0';
    converted[31] = '\0';
    char *toInsert = &converted[30];
    u64 valueCopy = value;

    // do conversion
    while(valueCopy != 0) {
        *toInsert = translation[valueCopy % base];
        valueCopy /= base;
        toInsert--;
    }

    // print value
    print(toInsert + 1);

    // if we printed hex, disable this state
    shouldNextNumberBeHex = false;

}

void Logger::print(const i32 value) {

    // if for some reason, we don't have backing device, just return
    if(currentOutputDevice == nullptr) return;

    // if number is zero, just print 0
    if(value == 0) {
        print('0');
        return;
    }

    // if value must be hexadecimal, ignore rest of the code, convert it to u32 and call Print(u32)
    if(shouldNextNumberBeHex) {
        print(static_cast<u32>(value));
        return;
    }

    // if value is INT_MIN, just print it
    if(value == -2147483648) { 
        
        print("-2147483648");
        return;
    }

    // in other case, convert it to positive and call Print(u32)
    if(value < 0) {
        print('-');
        print(static_cast<u32>(-value));
    }
    else print(static_cast<u32>(value));

}

void Logger::print(const i64 value) {

    // if for some reason, we don't have backing device, just return
    if(currentOutputDevice == nullptr) return;

    // if number is zero, just print 0
    if(value == 0) {
        print('0');
        return;
    }

    // if value must be hexadecimal, ignore rest of the code, convert it to u32 and call Print(u32)
    if(shouldNextNumberBeHex) {
        print(static_cast<u64>(value));
        return;
    }

    // if value is INT_MIN, just print it
    if(value == -9223372036854775807) { 
        print("-9223372036854775807");
        return;
    }

    // in other case, convert it to positive and call Print(u32)
    if(value < 0) {
        print('-');
        print(static_cast<u64>(-value));
    }
    else print(static_cast<u64>(value));

}

void Logger::print(const bool value) {

    // if for some reason, we don't have backing device, just return
    if(currentOutputDevice == nullptr) return;

    // print boolean
    print(value ? "<true>" : "<false>");

}

