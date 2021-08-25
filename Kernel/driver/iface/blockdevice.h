#pragma once
#include <util/types.h>
#include <mem/vas.h>

/**
 * @brief Interface encapsulating block device
 */
class IBlockDevice {

public:
    /**
     * @brief Returns whether the device is writeable or not
     * @return true if device is writeable, false otherwise
     */
    virtual bool isWriteable() = 0;

    /**
     * @brief Returns sector count of the device
     * @return Sector count of the device
     */
    virtual usz sectorCount() = 0;

    /**
     * @brief Reads specified sectors from block device
     * @param sector Sector to start disk access
     * @param count How many sectors to read
     * @param buffer Buffer where the data should be read
     * @param handler Callback to be called after reading
     * @param handlerData Data to passed to callback function
     * @return true if request was successfully sent, false otherwise
     */
    virtual bool read(usz sector, usz count, VirtualMemoryObject *buffer, EventHandler handler, void *handlerData) = 0;

    /**
     * @brief Writes specified sectors to block device
     * @param sector Sector to start disk access
     * @param count How many sectors to write
     * @param buffer Buffer from where the data should be written
     * @param handler Callback to be called after writing
     * @param handlerData Data to passed to callback function
     * @return true if request was successfully sent, false otherwise
     */
    virtual bool write(usz sector, usz count, VirtualMemoryObject *buffer, EventHandler handler, void *handlerData) = 0;

};