#pragma once
#include <util/types.h>
#include <driver/arch/portio.h>
#include <driver/iface/itxtout.h>

/**
 * @brief Class for interfacing with serial port
 */
class SerialPort : public ITextOutput {

public:

	enum class BaudRate : u8 {
		Baud115200 = 1,
		Baud9600 = 12
	};

	// bits constants
	static constexpr u8 Bits5 = 0b00 << 0;
	static constexpr u8 Bits6 = 0b01 << 0;
	static constexpr u8 Bits7 = 0b10 << 0;
	static constexpr u8 Bits8 = 0b11 << 0;

	// stop bits constants
	
	static constexpr u8 StopBits1 = 0b0 << 2;
	static constexpr u8 StopBits2 = 0b1 << 2;

	// parity constants
	static constexpr u8 ParityNone = 0b000 << 3;
	static constexpr u8 ParityOdd = 0b001 << 3;
	static constexpr u8 ParityEven = 0b011 << 3;
	static constexpr u8 ParityMark = 0b101 << 3;
	static constexpr u8 ParitySpace = 0b111 << 3;

	/**
	 * @brief Constructor
	 * @param baudRate Baud rate of the port
	 * @param settings Miscellaneous settings of port
	 */
	SerialPort(u16 basePort, BaudRate baudRate = BaudRate::Baud115200, u8 settings = (Bits8 | StopBits1 | ParityNone));

	/**
	 * @brief Returns whether port was successfully initialized
	 * @return true if port was successfully initialized, false otherwise
	 */
	bool initializedCorrectly();

	/**
     * @brief Implementation of interface function
     * @param codepoint Codepoint of character to be output
     */
	void outputCharacter(u32 codepoint) override;

private:

	static constexpr u16 OffsetDataRegister = 0;
	static constexpr u16 OffsetDivisorLowRegister = 0;
	static constexpr u16 OffsetDivisorHighRegister = 1;
	static constexpr u16 OffsetInterruptEnableRegister = 1;
	static constexpr u16 OffsetInterruptFIFORegister = 2;
	static constexpr u16 OffsetLineControlRegister = 3;
	static constexpr u16 OffsetModemControlRegister = 4;
	static constexpr u16 OffsetLineStatusRegister = 5;
	static constexpr u16 OffsetModemStatusRegister = 6;
	static constexpr u16 OffsetScratchRegister = 7;

	u16 basePort;
	BaudRate baudRate;
	u8 settings;
	bool initSuccessful = true;

};

