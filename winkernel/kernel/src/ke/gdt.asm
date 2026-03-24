; WinKernel NTKRNL-X — GDT flush routines (Intel syntax, 64-bit)

bits 64
default rel

global _GdtFlush
global _TssFlush

; _GdtFlush(QWORD GdtPtr, WORD CodeSel, WORD DataSel)
;   rdi = pointer to GDT_POINTER struct
;   rsi = kernel code selector
;   rdx = kernel data selector
_GdtFlush:
    lgdt [rdi]

    ; Reload data segment registers
    mov ax, dx
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far return to reload CS
    pop rax                 ; return address
    push rsi                ; new CS
    push rax                ; return address
    retfq

; _TssFlush(WORD TssSel)
;   rdi = TSS selector
_TssFlush:
    ltr di
    ret
