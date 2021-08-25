#include "driver/acpi/acpibase.h"

void ACPI::initialize() {

    // get pointer and adjust it
    xsdt = reinterpret_cast<XSDT*>(BootBoot::getStructure().acpiPointer);
    if(xsdt == nullptr) {
        Logger::printFormat("[acpibase] XSDT not found, aborting...\n");
        for(;;);
        // TODO: panic! 
    }

    Logger::printFormat("[acpibase] XSDT found, verifying...\n");

    // validate xsdt
    if(!validate(xsdt)) {
        Logger::printFormat("[acpibase] XSDT is not valid, aborting...\n");
        for(;;);
        // TODO: panic! 
    }

    // create table list
    tables = new List<Table*>;
    Logger::printFormat("[acpibase] XSDT valid, listing all tables...\n");

    // list all tables contained in xsdt
    usz entrySize = (xsdt->signature[0] == 'X') ? 8 : 4;
    usz entryCount = (xsdt->length - sizeof(Table)) / entrySize;
    usz printed = 0;
    Logger::printFormat("[acpibase] listing all tables in XSDT: \n");

    // 64-bit addresses
    if(entrySize == 8) {
        u64 *entries = reinterpret_cast<u64*>(&xsdt->pointers);
        for(usz i = 0; i < entryCount; i++) {

            // get header
            Table *header = reinterpret_cast<Table*>(entries[i] + CPU::pagingBase);

            // verify table checksum
            if(!validate(header)) {
                Logger::printFormat("[acpibase] found invalid table with signature: %c%c%c%c\n", header->signature[0], header->signature[1], header->signature[2], header->signature[3]);
                continue;
            }

            // print found table
            Logger::printFormat("[acpibase]   - %c%c%c%c at 0x%x\n", header->signature[0], header->signature[1], header->signature[2], header->signature[3], reinterpret_cast<u64>(header));
            printed++;

            // if valid, append it to table list
            tables->appendBack(header);

        }
    }

    // 32-bit addresses
    else {
        u32 *entries = reinterpret_cast<u32*>(&xsdt->pointers);
        for(usz i = 0; i < entryCount; i++) {

            // get header
            Table *header = reinterpret_cast<Table*>(entries[i] + CPU::pagingBase);

            // verify table checksum
            if(!validate(header)) {
                Logger::printFormat("[acpibase] found invalid table with signature: %c%c%c%c\n", header->signature[0], header->signature[1], header->signature[2], header->signature[3]);
                continue;
            }

            // print found table
            Logger::printFormat("[acpibase]   - %c%c%c%c at 0x%x\n", header->signature[0], header->signature[1], header->signature[2], header->signature[3], reinterpret_cast<u64>(header));
            printed++;

            // if valid, append it to table list
            tables->appendBack(header);

        }
    }

    if(printed == 0) Logger::printFormat("[acpibase] no tables found...\n");

}

bool ACPI::validate(Table *table) {

    // perform ACPI specified checksum validation
    usz length = table->length;
    u8 *bytes = reinterpret_cast<u8*>(table);
    usz value = 0;

    // calculate value and return result
    for(usz i = 0; i < length; i++) value += bytes[i];
    return (value & 0xff) == 0;

}

ACPI::Table *ACPI::getTableBySignature(const char *signature) {

    // iterate through list
    for(usz i = 0; i < tables->size(); i++) {

        // get table
        Table *table = tables->get(i);

        // compare signatures
        bool good = true;
        for(int i = 0; i < 4; i++) if(signature[i] != table->signature[i]) good = false;

        // return if found
        if(good) return table;

    }

    // return nullptr if not found
    return nullptr;

}