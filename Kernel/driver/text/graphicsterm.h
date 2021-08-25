#pragma once
#include <util/types.h>
#include <util/logger.h>
#include <util/bootboot.h>
#include <driver/iface/itxtout.h>

/**
 * Class for managing emulated graphics terminal
 */
class GraphicsTerminal : public ITextOutput {

public:

    /**
     * @brief Constructor
     * @param framebuffer Virtual address of framebuffer
     * @param framebufferWidth Width in pixels of given framebuffer
     * @param framebufferHeight Height in pixels of given framebuffer
     * @param framebufferPitch Pitch in bytes of given framebuffer
     * @param framebufferType Type of color of the framebuffer
     */
    GraphicsTerminal(void *framebuffer, u32 framebufferWidth, u32 framebufferHeight, u32 framebufferPitch, BootBoot::FramebufferType framebufferType);

    /**
     * @brief Implementation of interface function
     * @param codepoint Codepoint of character to be output
     */
    void outputCharacter(u32 codepoint) override;

private:

    void renderCharacter(u8 codepoint);
    void putPixel(u32 x, u32 y, Color c);

    u32 textWidth;
    u32 textHeight;
    void *framebuffer = nullptr;
    u32 framebufferWidth;
    u32 framebufferHeight;
    u32 framebufferPitch;
    BootBoot::FramebufferType framebufferType;

    u32 currentX;
    u32 currentY;

};