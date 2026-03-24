; WinKernel NTKRNL-X — IDT stubs + common handler (NASM, 64-bit)

bits 64
default rel

global _IdtFlush
global _IsrStubTable

extern KiCommonHandler

; ── Load IDT ─────────────────────────────────────────────────────────────────
_IdtFlush:
    lidt [rdi]
    ret

; ── Stub macros ───────────────────────────────────────────────────────────────
%macro ISR_NOERR 1
isr_stub_%1:
    push qword 0        ; fake error code
    push qword %1       ; vector number
    jmp  _isr_common
%endmacro

%macro ISR_ERR 1
isr_stub_%1:
    ; CPU already pushed error code
    push qword %1       ; vector number
    jmp  _isr_common
%endmacro

; ── Exception stubs 0-31 ─────────────────────────────────────────────────────
ISR_NOERR  0
ISR_NOERR  1
ISR_NOERR  2
ISR_NOERR  3
ISR_NOERR  4
ISR_NOERR  5
ISR_NOERR  6
ISR_NOERR  7
ISR_ERR    8
ISR_NOERR  9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

; ── IRQ stubs 32-47 ──────────────────────────────────────────────────────────
%assign i 32
%rep 16
ISR_NOERR i
%assign i i+1
%endrep

; ── Vectors 48-255 ───────────────────────────────────────────────────────────
%assign i 48
%rep 208
ISR_NOERR i
%assign i i+1
%endrep

; ── Common handler ────────────────────────────────────────────────────────────
; Stack layout on entry (rsp points at vector number):
;   [rsp+ 0]  vector
;   [rsp+ 8]  error code
;   [rsp+16]  RIP    \
;   [rsp+24]  CS      | CPU frame
;   [rsp+32]  RFLAGS /
;   (RSP/SS only present on privilege change — we stay in ring 0)
;
; KTRAP_FRAME C layout (first field = lowest address after all pushes):
;   R15 R14 R13 R12 R11 R10 R9 R8 RBP RDI RSI RDX RCX RBX RAX
;   InterruptNumber ErrorCode RIP CS RFLAGS RSP SS

_isr_common:
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    ; First arg = pointer to KTRAP_FRAME
    mov  rdi, rsp

    ; Align stack to 16 bytes for ABI compliance
    ; We pushed 15 regs (120 bytes) + vector + errcode (16) = 136 bytes
    ; CPU pushed RIP+CS+RFLAGS = 24 bytes (same ring, no RSP/SS)
    ; Total = 160 bytes. 160 % 16 = 0. Already aligned.
    call KiCommonHandler

    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    add rsp, 16     ; discard vector + error code
    iretq

; ── Stub pointer table (in .data so it gets the kernel's virtual address) ────
section .data
_IsrStubTable:
%assign i 0
%rep 256
    dq isr_stub_%+i
%assign i i+1
%endrep
