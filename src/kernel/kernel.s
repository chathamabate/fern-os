;; The Kernel Segment.
;; Will go from 0x10000 - 0x1FFFF
;;
;; This is 128 sectors in total!
;; LBA 1 - 128
;;
;; Other areas will be decided upon later!
org 0x10000
bits 16

%define KERNEL_SEG 0x1000


;; NOTE: from this point on, the boot sector will
;; be deprecated. DON'T USE IT'S FUNCTIONS!

    ;; Skip over system calls.
    jmp start_kernel

;; From this point on, functions are only
;; expected to preserve the cs and ss segment registers.
;; Parameters should be passed through the stack.

;; System Functions!
%include "src/kernel/str.s"
%include "src/kernel/tui.s"

start_kernel:
    call tui_init
    
    ;; Let's place a string on the stack bois.
    mov bp, sp
    mov byte [bp-1], 0
    mov byte [bp-2], "b"
    mov byte [bp-3], "a"
    mov byte [bp-4], "c"
    sub sp, 4 

    ;; ax point to the beginning of the string.
    mov ax, sp

    ;; Set up puts stack frame.
    mov bp, sp
    sub sp, 7

    mov byte [bp-1], 1
    mov byte [bp-2], 0
    mov byte [bp-3], COLOR(WHITE, BLACK)
    mov word [bp-5], ss
    mov word [bp-7], ax
    
    call tui_puts

.halt:
    hlt
    jmp .halt

msg: db "Hello", 0


;; This tells us how many bytes we have left
;; in the kernel segment.
%define BYTES_LEFT 65536-($-$$)
