#pragma once
#include <driver/iface/itxtout.h>
#include <util/types.h>
#include <util/spinlock.h>

/**
 * Basic logger for use in kernel
 */
class Logger {
public:

    static void setBackingDevice(ITextOutput *device);
    static void printFormat(const char *format);

	template<typename T, typename... Args>
    static void printFormat(const char *format, T value, Args... args) {

        // if current depth is 0, print color code
        if(printRecursionDepth == 0) {
			loggerSpinlock.lock();
		}

        // iterate through given format
        while(*format != '\0') {

            // assign helper variable
            char current = *format;

            // if current character is % do special magic
            if(current == '%') {

                // get format type
                format++;
                if(*format == '\0') {
                    print("<invalid format>");
                    if(printRecursionDepth == 0) {
                        print("\u001b[0m");
                        loggerSpinlock.unlock();
					}
                    return;
                }

                // acctually print the value
                char formatType = *format;
                switch(formatType) {
                    case 'c':
                    case 's':
                    case 'd':
                    case 'i':
                    case 'u':
                    case 'b':
                        print(value);
                        printRecursionDepth++;
                        printFormat(format + 1, args...);
                        printRecursionDepth--;
                        if(printRecursionDepth == 0) {
							loggerSpinlock.unlock();
						}
                        return;
                    case 'x':
                        shouldNextNumberBeHex = true;
                        print(value);
                        printRecursionDepth++;
                        printFormat(format + 1, args...);
                        printRecursionDepth--;
                        if(printRecursionDepth == 0) {
							loggerSpinlock.unlock();
						}
                        return;
                    default:
                        print("<invalid format>");
                        break;
                }

            }

            // else, just print the character
            else print(*format);

            // increment format
            format++;

        }

        // print color reset
        if(printRecursionDepth == 0) {
            loggerSpinlock.unlock();
        }

    }

private:

    static constexpr const char *decimalTranslation = "0123456789";
    static constexpr const char *hexadecimalTranslation = "0123456789abcdef";

    static inline ITextOutput *currentOutputDevice = nullptr;
    static inline bool shouldNextNumberBeHex = false;
    static inline usz printRecursionDepth = 0;
    static inline Spinlock loggerSpinlock;

    static void print(const char character);
    static void print(const char *string);
    static void print(const u32 value);
    static void print(const u64 value);
    static void print(const i32 value);
    static void print(const i64 value);
    static void print(const bool value);

};
