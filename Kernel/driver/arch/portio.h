#pragma once
#include <util/types.h>

/**
 * @brief Class for interacting with legacy port interface of CPU
 */
class PortIO {

public:

	/**
	 * @brief Outputs 8-bit value to specified port
	 * @param port Port to which the data should be output
	 * @param data Data to be output
	 */
	static void outputToPort8(u16 port, u8 data);

	/**
	 * @brief Outputs 16-bit value to specified port
	 * @param port Port to which the data should be output
	 * @param data Data to be output
	 */
	static void outputToPort16(u16 port, u16 data);

	/**
	 * @brief Outputs 32-bit value to specified port
	 * @param port Port to which the data should be output
	 * @param data Data to be output
	 */
	static void outputToPort32(u16 port, u32 data);

	/**
	 * @brief Inputs 8-bit value from specified port
	 * @param port Port from which the data should be input
	 * @return Input data
	 */
	static u8 inputFromPort8(u16 port);

	/**
	 * @brief Inputs 16-bit value from specified port
	 * @param port Port from which the data should be input
	 * @return Input data
	 */
	static u16 inputFromPort16(u16 port);

	/**
	 * @brief Inputs 32-bit value from specified port
	 * @param port Port from which the data should be input
	 * @return Input data
	 */
	static u32 inputFromPort32(u16 port);

};

