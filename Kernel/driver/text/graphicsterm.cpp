#include "driver/text/graphicsterm.h"
#include "driver/text/graphicsfont.h"


GraphicsTerminal::GraphicsTerminal(void *framebuffer, u32 framebufferWidth, u32 framebufferHeight, u32 framebufferPitch, BootBoot::FramebufferType framebufferType) 
    : framebuffer(framebuffer), framebufferWidth(framebufferWidth), framebufferHeight(framebufferHeight), framebufferPitch(framebufferPitch), framebufferType(framebufferType) {

    // calculate values
    textWidth = framebufferWidth / 8;
    textHeight = framebufferHeight / 8;
    currentX = 0;
    currentY = 0;

}

void GraphicsTerminal::outputCharacter(u32 codepoint) {
    
    // cast codepoint to u8
    u8 character = static_cast<u8>(codepoint);
    if(character > 127) character = 0;

    // render
    renderCharacter(character);


}

void GraphicsTerminal::renderCharacter(u8 codepoint) {

    // if codepoint is new line, just go to new line
    if(codepoint == static_cast<u8>('\n')) {
        currentY++;
        currentX = 0;
        if(currentY == textHeight) {
            currentX = 0;
            currentY = 0;
        }
        return;
    }

    for(usz i = 0; i < 8; i++) {
        u32 row = currentY * 8; 
        for(usz j = 0; j < 8; j++) {
            u32 column = currentX * 8;
            putPixel(column + j, row + i, (font8x8[codepoint][i] & (1 << j)) ? colorWhite : colorBlack);
        }
    }

    currentX++;
    if(currentX == textWidth) {
        currentX = 0;
        currentY++;
    }
    if(currentY == textHeight) {
        currentX = 0;
        currentY = 0;
    }

    // TODO: implement scrolling

}

void GraphicsTerminal::putPixel(u32 x, u32 y, Color c) {

    // put pixel
    u8 *where = reinterpret_cast<u8*>(reinterpret_cast<usz>(framebuffer) + y * framebufferPitch + 4 * x);

    // decide on framebuffer type
    if(framebufferType == BootBoot::FramebufferType::RGBA) {
        where[3] = c.red;
        where[2] = c.green;
        where[1] = c.blue;
        where[0] = 255;
    }
    else if(framebufferType == BootBoot::FramebufferType::BGRA) {
        where[3] = c.blue;
        where[2] = c.green;
        where[1] = c.red;
        where[0] = 255;
    }
    else if(framebufferType == BootBoot::FramebufferType::ARGB) {
        where[3] = 255;
        where[2] = c.red;
        where[1] = c.green;
        where[0] = c.blue;
    }
    else if(framebufferType == BootBoot::FramebufferType::ABGR) {
        where[3] = 255;
        where[2] = c.blue;
        where[1] = c.green;
        where[0] = c.red;
    }

}