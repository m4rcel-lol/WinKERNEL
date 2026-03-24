; WinKernel NTKRNL-X — IDT stub trampolines (64-bit, NASM Intel syntax)

bits 64
default rel

global _IdtFlush
global _IsrStubTable

extern KiCommonHandler

; ── _IdtFlush(rdi = &IDT_POINTER) ────────────────────────────────────────────
_IdtFlush:
    lidt [rdi]
    ret

; ── Stub macros ───────────────────────────────────────────────────────────────
; For exceptions WITHOUT a CPU-pushed error code: push 0 first, then vector.
; For exceptions WITH    a CPU-pushed error code: just push vector.
; After the stub the stack looks like:
;   [rsp+ 0]  vector
;   [rsp+ 8]  error code  (0 or CPU-pushed)
;   [rsp+16]  RIP   \
;   [rsp+24]  CS     |  CPU-pushed interrupt frame
;   [rsp+32]  RFLAGS |
;   [rsp+40]  RSP   /  (only present on privilege change; same-ring: absent)
;   [rsp+48]  SS

%macro ISR_NOERR 1
isr_stub_%1:
    push qword 0
    push qword %1
    jmp  _isr_common
%endmacro

%macro ISR_ERR 1
isr_stub_%1:
    push qword %1
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

; ── Remaining vectors 48-255 ─────────────────────────────────────────────────
%assign i 48
%rep 208
ISR_NOERR i
%assign i i+1
%endrep

; ── Common handler ────────────────────────────────────────────────────────────
; KTRAP_FRAME C struct layout (first field = lowest address = top of stack):
;   R15 R14 R13 R12 R11 R10 R9 R8  RBP RDI RSI RDX RCX RBX RAX
;   InterruptNumber  ErrorCode
;   RIP  CS  RFLAGS  RSP  SS        <- CPU-pushed
;
; We push GPRs in the order that matches the struct (R15 first = top of stack).

_isr_common:
    ; Save all GPRs — order must match KTRAP_FRAME exactly
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

    ; rsp now points to the base of KTRAP_FRAME — pass as first arg
    mov  rdi, rsp

    ; System V AMD64 ABI requires 16-byte stack alignment before call.
    ; We have pushed 15 GPRs (15*8=120) + vector + errcode (2*8=16) = 136 bytes
    ; plus the CPU frame (5*8=40) = 176 bytes total.
    ; 176 % 16 = 0, so we are already aligned. Just call directly.
    call KiCommonHandler

    ; Restore GPRs
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

    ; Discard vector number and error code
    add rsp, 16

    iretq

; ── Stub address table ────────────────────────────────────────────────────────
section .rodata
_IsrStubTable:
%assign i 0
%rep 256
    dq isr_stub_%+i
%assign i i+1
%endrep
