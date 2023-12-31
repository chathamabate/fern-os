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
;; expected to preserve the cs, ss, sp, and bp registers.
;; Parameters should be passed through the stack.
;; Parameters should not be modified by the called function.
;; (Only Read)
;;
;; "return values" are simulated by providing 
;; a pointer to the desired return location.
;;
;; Lastly, even in the function has no paramters,
;; bp should be set to sp before calling the function.

;; System Functions!
%include "src/kernel/str.s"
%include "src/kernel/tui.s"

start_kernel:
    ;; prepare for call.
    mov bp, sp
    call tui_init
    
    ;; Let's place a string on the stack bois.
    mov bp, sp
    sub sp, 7

    mov byte [bp-1], 0
    mov byte [bp-2], "_"
    mov byte [bp-3], "_"
    mov byte [bp-4], "_"
    mov byte [bp-5], "_"
    mov byte [bp-6], "_"
    mov byte [bp-7], "_"

    ;; ax point to the beginning of the string buffer.
    mov ax, sp

    ;; Set up u8 to hex stack frame. 
    mov bp, sp
    sub sp, 6
    mov [bp-2], ss
    mov [bp-4], ax
    mov word [bp-6], (1<<15)

    call str_s16_to_dec
    
    ;; dispose of stack frame.
    mov sp, bp

    ;; Reload our string buffer reference.
    mov ax, sp

    ;; Set up puts stack frame.
    mov bp, sp
    sub sp, 7

    mov byte [bp-1], 0
    mov byte [bp-2], 0
    mov byte [bp-3], COLOR(GREEN, BLACK)
    mov word [bp-5], ss
    mov word [bp-7], ax
    
    call tui_puts

.halt:
    hlt
    jmp .halt

;; This tells us how many bytes we have left
;; in the kernel segment.
%define BYTES_LEFT 65536-($-$$)
