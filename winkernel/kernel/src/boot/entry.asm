; WinKernel NTKRNL-X — kernel entry point
; Limine jumps here in 64-bit long mode with paging already enabled.

bits 64
default rel

global _start
extern KiSystemStartup

; ── BSS symbols from linker script ──────────────────────────────────────────
extern __bss_start
extern __bss_end

section .text.boot

_start:
    ; Disable interrupts immediately
    cli

    ; Load our own stack (32 KB, defined below in .bss)
    lea rsp, [kernel_stack_top]

    ; Zero the BSS segment
    lea rdi, [__bss_start]
    lea rcx, [__bss_end]
    sub rcx, rdi
    xor eax, eax
    rep stosb

    ; Call the C kernel entry — should never return
    call KiSystemStartup

    ; If KiSystemStartup somehow returns, halt forever
.hang:
    cli
    hlt
    jmp .hang

section .bss
align 16
kernel_stack_bottom:
    resb 32768          ; 32 KB kernel stack
kernel_stack_top:
