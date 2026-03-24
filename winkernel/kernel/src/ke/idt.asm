; WinKernel NTKRNL-X — IDT stub trampolines (Intel syntax, 64-bit)
; Generates 256 interrupt stubs that push vector number + call common handler

bits 64
default rel

global _IdtFlush
global _IsrStubTable

extern KiCommonHandler

; ── IDT load ────────────────────────────────────────────────────────────────
; _IdtFlush(QWORD IdtPtr)
_IdtFlush:
    lidt [rdi]
    ret

; ── Macro: exception stub WITH error code already on stack ──────────────────
%macro ISR_ERR 1
isr_stub_%1:
    push qword %1       ; interrupt number
    jmp  _isr_common
%endmacro

; ── Macro: exception stub WITHOUT error code — push dummy 0 ─────────────────
%macro ISR_NOERR 1
isr_stub_%1:
    push qword 0        ; dummy error code
    push qword %1       ; interrupt number
    jmp  _isr_common
%endmacro

; ── Exception stubs 0–31 ────────────────────────────────────────────────────
ISR_NOERR  0    ; #DE Divide Error
ISR_NOERR  1    ; #DB Debug
ISR_NOERR  2    ; NMI
ISR_NOERR  3    ; #BP Breakpoint
ISR_NOERR  4    ; #OF Overflow
ISR_NOERR  5    ; #BR Bound Range
ISR_NOERR  6    ; #UD Invalid Opcode
ISR_NOERR  7    ; #NM Device Not Available
ISR_ERR    8    ; #DF Double Fault        (has error code)
ISR_NOERR  9    ; Coprocessor Segment Overrun
ISR_ERR   10    ; #TS Invalid TSS         (has error code)
ISR_ERR   11    ; #NP Segment Not Present (has error code)
ISR_ERR   12    ; #SS Stack Fault         (has error code)
ISR_ERR   13    ; #GP General Protection  (has error code)
ISR_ERR   14    ; #PF Page Fault          (has error code)
ISR_NOERR 15    ; Reserved
ISR_NOERR 16    ; #MF x87 FPU Error
ISR_ERR   17    ; #AC Alignment Check     (has error code)
ISR_NOERR 18    ; #MC Machine Check
ISR_NOERR 19    ; #XM SIMD Exception
ISR_NOERR 20    ; #VE Virtualization
ISR_ERR   21    ; #CP Control Protection  (has error code)
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30    ; #SX Security Exception  (has error code)
ISR_NOERR 31

; ── IRQ stubs 32–47 (PIC remapped) ──────────────────────────────────────────
%assign i 32
%rep 16
ISR_NOERR i
%assign i i+1
%endrep

; ── Remaining vectors 48–255 ────────────────────────────────────────────────
%assign i 48
%rep 208
ISR_NOERR i
%assign i i+1
%endrep

; ── Common handler: save all registers, call C handler, restore ──────────────
_isr_common:
    ; At this point the stack contains (top to bottom):
    ;   [rsp+0]  = interrupt number
    ;   [rsp+8]  = error code
    ;   [rsp+16] = RIP  (CPU-pushed)
    ;   [rsp+24] = CS
    ;   [rsp+32] = RFLAGS
    ;   [rsp+40] = RSP  (CPU-pushed, if privilege change)
    ;   [rsp+48] = SS

    ; Save general-purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Pass pointer to KTRAP_FRAME as first argument
    mov rdi, rsp
    call KiCommonHandler

    ; Restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Remove interrupt number + error code
    add rsp, 16

    iretq

; ── Stub address table ───────────────────────────────────────────────────────
section .rodata
_IsrStubTable:
%assign i 0
%rep 256
    dq isr_stub_%+i
%assign i i+1
%endrep
