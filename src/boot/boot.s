org 0x7C00

bits 16

%define ENDL 0x0D, 0x0A

;; Here is the boot sector.
;; We could consider printing out the disk geometry?
;; Could be a cool move for sure.

;; Here we use the memory before the boot sector as
;; its temporary stack. After booting, we will changed this.
init_raw_stack:
    mov ax, 0

    ;; NOTE: remember when using bp, the default segment used is ss.
    ;; when using si and di, the default segment used is ds.
    mov ss, ax
    mov ds, ax

    mov bp, 0x7C00
    mov sp, 0x7C00

main:
    mov si, msg
    call puts

    hlt

.halt:
    jmp .halt

msg:
    db "Hello", ENDL, 0


;; Print a string.
;; Params:
;;  si: pointer to the start of the string.
puts:
    push si
    push ax

    mov ah, 0x0E

.loop:
    mov al, [si]

    cmp al, 0
    je .exit

    int 0x10
    
    inc si
    jmp .loop

.exit:
    pop ax
    pop si
    ret


times 510-($-$$) db 0

;; Boot signature.
dw 0xAA55
