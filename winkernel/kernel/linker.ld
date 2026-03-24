/* WinKernel NTKRNL-X linker script — kernel mapped at -2GB (0xFFFFFFFF80000000) */

OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS {
    . = 0xFFFFFFFF80000000;

    .text ALIGN(0x1000) : {
        KEEP(*(.text.boot))
        *(.text .text.*)
    }

    .rodata ALIGN(0x1000) : {
        *(.rodata .rodata.*)
    }

    .data ALIGN(0x1000) : {
        *(.data .data.*)
    }

    /* Limine requests must be in a writable section */
    .limine_requests ALIGN(0x1000) : {
        KEEP(*(.limine_requests))
    }

    .bss ALIGN(0x1000) : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    __kernel_end = .;

    /DISCARD/ : {
        *(.eh_frame)
        *(.note .note.*)
    }
}
