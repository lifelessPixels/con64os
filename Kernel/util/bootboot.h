#pragma once
#include <driver/arch/cpu.h>
#include <util/types.h>

/**
 * @brief Class for interfacing with BOOTBOOT protocol
 */
class BootBoot {

public:

    static constexpr const char *magicString = "BOOT";

    enum class FramebufferType : u8 {
		ARGB = 0,
		RGBA = 1,
		ABGR = 2,
		BGRA = 3
    };

    struct MemoryMapEntry {

        enum class Type : u8 {
			used = 0,
			free = 1,
			acpi = 2,
			mmio = 3
        };

        inline u64 getAddress() { return this->pointer; };
        inline u64 getSize() { return (this->size & 0xfffffffffffffff0); };
        inline Type getType() { return static_cast<Type>(this->size & 0x0f); };
        inline bool isFree() { return getType() == Type::free; };
        inline void setAddress(u64 address) { this->pointer = address; };
        inline void setSize(u64 size) { this->size = (size & 0xfffffffffffffff0) | static_cast<u8>(this->getType()); };
        inline void setType(Type type) { this->size = (this->size & 0xfffffffffffffff0) | static_cast<u8>(type); };

    private:
        u64 pointer;
        u64 size;

    };
    
    struct Structure {
        u8 magic[4];
        u32 size;
        u8 protocol;
        FramebufferType framebufferType;
        u16 coreCount;
        u16 bspID;
        i16 timezone;
        u8 datetime[8];
        u64 initrdPointer;
        u64 initrdSize;
        u64 framebufferPointer;
        u32 framebufferSize;
        u32 framebufferWidth;
        u32 framebufferHeight;
        u32 framebufferScanline;
        u64 acpiPointer;
        u64 smbiosPointer;
        u64 efiPointer;
        u64 mpPointer;
        u64 unused0;
        u64 unused1;
        u64 unused2;
        u64 unused3;
        MemoryMapEntry memoryMap[];
    };
    
    static void registerStructure(Structure * structure) { 
    
		// adjust pointers by paging base
		globalStructure = structure;
		globalStructure->initrdPointer += CPU::pagingBase;
		globalStructure->framebufferPointer += CPU::pagingBase;
		globalStructure->acpiPointer += CPU::pagingBase;
		globalStructure->smbiosPointer += CPU::pagingBase;
		globalStructure->efiPointer += CPU::pagingBase;
		globalStructure->mpPointer += CPU::pagingBase; 
		
	};

    static inline Structure& getStructure() { return *globalStructure; };

private:
    static inline Structure *globalStructure;

};
