#pragma once
#include <util/types.h>

/**
 * Class encapsulating text output device
 */
class ITextOutput {
	
public:
	/**
	 * @brief Outputs specified character to the device
	 * @param codepoint Character code to be output
	 */
	virtual void outputCharacter(u32 codepoint) = 0;

};
