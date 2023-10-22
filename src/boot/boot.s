org 0x7C00

bits 16

%define ENDL 0x0D, 0x0A


;; Here is the boot sector.
;; We could consider printing out the disk geometry?
;; Could be a cool move for sure.

main:
    ;; Should just print H.
    mov ah, 0x0E
    mov al, 'H'
    int 0x10

    hlt

.halt:
    jmp .halt

times 510-($-$$) db 0

;; Boot signature.
dw 0xAA55
