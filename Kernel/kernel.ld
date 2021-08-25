ENTRY(kernelMain)

/* bootboot stuff */
mmio        = 0xfffffffff8000000;              
fb          = 0xfffffffffc000000;
bootboot    = 0xffffffffffe00000;
environment = 0xffffffffffe01000;
initstack   = 4096; 

PHDRS {
    boot PT_LOAD;
}

SECTIONS
{
    . = 0xffffffffffe02000;

    kernelCodeStart = .;
    .text : ALIGN(4096) {
        KEEP(*(.text.boot)) *(.text .text.*)
    } :boot
    . = ALIGN(4096);
    kernelCodeEnd = .;

    kernelRodataStart = .;
    .rodata : ALIGN(4096) {
        *(.rodata .rodata.*)
    } :boot
    . = ALIGN(4096);
    kernelRodataEnd = .;

    .data : ALIGN(4096) {
        *(.data .data.*)
    } :boot

    .bss (NOLOAD) : ALIGN(4096) {
        *(.bss .bss.*)
        *(COMMON)
    } :boot

    /DISCARD/ : { *(.eh_frame) *(.comment) }
}
