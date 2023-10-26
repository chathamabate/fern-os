;; The Kernel Segment.
;; Will go from 0x10000 - 0x1FFFF
;;
;; This is 128 sectors in total!
;; LBA 1 - 128
;;
;; Other areas will be decided upon later!
org 0x10000

bits 16

;; NOTE: from this point on, the boot sector will
;; be deprecated. DON'T USE IT'S FUNCTIONS!

init_segments:
    ;; Set up kernel segments.
    mov ax, 0x1000
    mov ds, ax
    mov es, ax
    
    jmp start_kernel

;; From this point on, functions are only
;; expected to preserve segment register values.
;; Parameters should be passed through the stack.

;; System Functions!
%include "src/kernel/io.s"

start_kernel:
    call tui_init
    ;; Kernel Start Up Code!
.halt:
    hlt
    jmp .halt


;; This tells us how many bytes we have left
;; in the kernel segment.
%define BYTES_LEFT 65536-($-$$)
