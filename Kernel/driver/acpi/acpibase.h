#pragma once
#include <mem/heap.h>
#include <util/bootboot.h>
#include <util/logger.h>
#include <util/types.h>
#include <util/list.h>

/**
 * @brief Class for managing ACPI tables exposed by platform firmware
 */ 

class ACPI {

public:

    /**
     * @brief Base structure of every ACPI table
     */
    struct Table {
        char signature[4];
        u32 length;
        u8 revision;
        u8 checksum;
        char oemID[6];
        char oemTableID[8];
        u32 oemRevision;
        u32 creatorID;
        u32 creatorRevision;
    } __attribute__((packed));

    /**
     * @brief ACPI-defined structure for address
     */
    struct GenericAddressStructure {
        u8 addressSpaceID;
        u8 registerBitWidth;
        u8 registerBitOffset;
        u8 reserved;
        u64 address;
    } __attribute__((packed));

    /**
     * @brief Initializes ACPI subsystem, get all tables, check their checksum and expose for dependent code
     */
    static void initialize();

    /**
     * @brief Gets ACPI table by its signature
     * @param signature 4-character signature of table to be found
     * @return Corresponding ACPI table
     */
    static Table *getTableBySignature(const char *signature);

private:

    struct XSDT : public Table {
        u8 pointers[1];
    } __attribute__((packed));

    static inline XSDT *xsdt = nullptr;
    static inline List<Table*> *tables;

    static bool validate(Table *table);
    
};