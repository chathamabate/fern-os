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
    mov ax, 0xCDE4
    mov di, msg
    call store_hex_str
    call store_hex_str
    
    mov si, msg
    call print_str

    hlt

.halt:
    jmp .halt



msg: db 0, 0, 0, 0, ENDL, 0

;; Stores the given registers value as a hex string
;; at the given location. (Null terminator is NOT
;; included)
;;
;; Params:
;;  di: the desination to store at.
;;  ax: the register to translate.
store_hex_str:
    push cx
    push bx
    push di

    ;; ax contains 16 bits (4 groups of 4)    
    ;; each group of 4 must be translated.
    
    ;; we are going to do 4 iterations.
    mov cl, 0

.loop:
    mov bx, ax

    sar bx, 12  ;; Will copy high bits.
    and bx, 0x000F

    cmp bx, 10
    jge .hex_digit

.decimal_digit: 
    add bl, "0"
    jmp .continue

.hex_digit:
    add bl, "A" - 10

.continue:
    mov [di], bl
    inc di

    rol ax, 4

    inc cl
    cmp cl, 4
    je .exit
    jmp .loop

.exit:
    pop di
    pop bx
    pop cx
     
    ret

;; Print a string.
;; Params:
;;  si: pointer to the start of the string.
print_str:
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
