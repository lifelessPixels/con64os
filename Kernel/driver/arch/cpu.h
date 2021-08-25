#pragma once
#include <util/types.h>
#include <cpuid.h>

/**
 * @brief Class for managing currently executing CPU
 */
class CPU {
public:

	/**
	 * @brief Base address of higher-half memory
	 */
	static constexpr u64 pagingBase = 0xffff800000000000;

	/**
	 * @brief Structure containing all values returned by CPU after CPUID query
	 */
	struct CPUID {
		u32 leaf;
		u32 aRegister;
		u32 bRegister;
		u32 cRegister;
		u32 dRegister;
	};

	/**
	 * @brief Structure encapsulting pointer to CPU tables like IDT and GDT
	 */
	struct Pointer {
		u16 size;
		u64 address;
	} __attribute__((packed));
	
	/**
	 * @brief Returns information about CPU from sepcified "leaf"
	 * @param leaf Leaf of info to be returned
	 * @return Corresponding data returned by CPU
	 */
	static CPUID getCPUID(u32 leaf);

	/**
	 * @brief Returns LAPIC ID of currently executing processor
	 * @return LAPIC ID of current processor
	 */
	static u8 getCoreAPICID();

	/**
	 * @brief Enables or disables servicing of interrupts
	 * @param interruptState true if interrupts should be enabled, false otherwise
	 */
	static void setInterruptState(bool interruptState);

	/**
	 * @brief Return info about servicing of interrupts
	 * @return true if interrupt servicing is enabled, false otherwise
	 */
	static bool getInterruptState();

	/**
	 * @brief Disables interrupts (for entering critical sections of code)
	 * @return Current interrupt state
	 */
	static bool enterCritical();

	/**
	 * @brief Exits critical section of code 
	 * @param interruptState Interrupt state before entering critical section of code
	 */
	static void exitCritical(bool interruptState);

	/**
	 * @brief Loads GDTR register
	 * @param pointer Pointer to GDT pointer structure
	 */
	static void loadGDT(Pointer pointer);

	/**
	 * @brief Loads IDTR register
	 * @param pointer Pointer to IDT pointer structure
	 */
	static void loadIDT(Pointer pointer);

	/**
	 * @brief Loads CS register
	 * @param segment Segment to be loaded
	 */
	static void loadCodeSegment(u16 segment);

	/**
	 * @brief Loads DS, ES, FS and GS registers
	 * @param segment Segment to be loaded
	 */
	static void loadDataSegments(u16 segment);
	
	/**
	 * @brief Returns current flags register contents
	 * @return Current flags register contents
	 */
	static u64 readEFLAGS();

	/**
	 * @brief Returns current CR3 contents
	 * @return Current CR3 register contents
	 */
	static u64 readCR3();

	/**
	 * @brief Writes CR3 contents
	 * @param value CR3 register contents to be written
	 */
	static void writeCR3(u64 value);

	/**
	 * @brief Invalidates TLB entry
	 * @param address Address to be invalidated
	 */
	static void invalidatePagingEntry(void *address);

	/**
	 * @brief Reads model specific register of CPU
	 * @param msr MSR address from where the data should be read
	 * @return Specified MSR contents
	 */
	static u64 readMSR(u32 msr);

	/**
	 * @brief Writes model specific register of CPU
	 * @param msr MSR address from where the data should be written
	 * @param value Data to be written to specified MSR
	 */
	static void writeMSR(u32 msr, u64 value);

	/**
	 * @brief Enables no-execution protection mechanism
	 */
	static void enableNXBit();

	/**
	 * @brief Enables system call extensions mechanism
	 */
	static void enableSystemCallExtensions();


private:
	static constexpr u32 eferMSRAddress = 0xc0000080;

};