org 0x7C00

bits 16

%define ENDL 0x0D, 0x0A

;; Here is the boot sector.
;; We could consider printing out the disk geometry?
;; Could be a cool move for sure.

;; Here we use the memory before the boot sector as
;; its temporary stack. After booting, we will changed this.
init_segs:
    mov ax, 0

    ;; NOTE: remember when using bp, the default segment used is ss.
    ;; when using si and di, the default segment used is ds.
    mov ss, ax
    mov ds, ax
    mov cs, ax
    mov es, ax

init_stack:
    mov bp, 0x7C00
    mov sp, 0x7C00

;; Here we will find and store traits about the drive 
;; we are booting from.
discover_drive:
    ;; prepare for interrupt.

    ;; set es:di = 0000:0000
    mov di, 0
    
    mov ah, 0x08
    mov dl, 0x80    ;; First drive index

    int 0x13

    ;; On error, the carry flag is set.
    jnc .continue

    mov si, drive_error
    call print_str

.halt:
    hlt
    jmp .halt

.continue:
    ;; Store number of heads.
    inc dh
    mov [tracks_per_cylinder], dh

    call expand_cs
    mov word [cylinders], bx
    mov byte [sectors_per_track], cl


;;    mov byte [cylinders], ch
;;    mov ch, cl
;;
;;    sar ch, 6
;;    and ch, 0b00000011
;;    mov byte [cylinders + 1], ch
;;    inc word [cylinders]
;;    
;;    and cl, 0b00111111
;;    mov byte [sectors_per_track], cl

    jmp main 

msg: db "Here", ENDL, 0
drive_error: db "Drive Err", ENDL, 0

tracks_per_cylinder: db 0   ;; = heads.
cylinders: dw 0         
sectors_per_track: db 0       

main:
    mov si, geometry_prefix
    call print_str
    
    mov ax, 0
    mov al, [tracks_per_cylinder]
    call print_dec_str
    call print_newline

    mov ax, [cylinders]
    call print_dec_str
    call print_newline

    mov ax, 0
    mov al, [sectors_per_track]
    call print_dec_str
    call print_newline

    mov si, done_msg
    call print_str
    call print_newline
    
    mov ax, 0
    mov ax, [bytes_left]
    call print_dec_str
    call print_newline

.halt:
    hlt
    jmp .halt

geometry_prefix: db "H/C/S:", ENDL, 0

done_msg: db "DONE ", 0

;; Sector = (lba % spt) + 1
;; Head = (lba / spt) % Heads
;; Cylinder = (lba / spt) / heads

;; Given Cylinder and Sector, expand into 2 different
;; registers.
;; Params:
;;  cx: cylinder/sector        
;;      NOTE: The 2 high bits of cl are actually
;;      the 9th and 10th high bits of the cylinder.
;;
;; Returns:
;;  cl: sector.
;;  bx: cylinder.
;;  (ch is left unchanged)
expand_cs:
    mov bh, cl  
    and cl, 0b00111111

    sar bh, 6
    and bh, 0b00000011
    mov bl, ch

    ret

;; Given expanded cylinder and sector, compress into
;; a single register.
;; registers.
;; Params:
;;  cl: sector.
;;  bx: cylinder.
;;
;; Returns:
;;  cx: cylinder/sector        
compress_cs:
    push bx 

    mov ch, bl
    sal bh, 6
    or cl, bh
    
    pop bx

    ret

;; Given CHS, convert to LBA.
;; Params:
;;  dh: head
;;  cx: cylinder/sector        
;;      NOTE: The 2 high bits of cl are actually
;;      the 9th and 10th high bits of the cylinder.
chs_to_lba:
    

    ret

;; Given LBA
;; I think this is a 2 register value...
lba_to_chs:
    ret

;; Stores the given register's value as a decimal string.
;; (NULL terminator is NOT included)
;; Writes 5 characters always.
;;
;; Params:
;;  di: the desination to store at.
;;  ax: the register to translate.
store_dec_str:
    push ax
    push bx
    push cx
    push dx

    mov cl, 5
    add di, 4

    mov bx, 10 ;; Denominator

.loop:
    mov dx, 0   ;; Remember dx:ax is used as the numerator.
    div bx

    add dl, "0"
    mov [di], dl

    dec cl
    jz .exit
    
    dec di
    jmp .loop

.exit:
    pop dx
    pop cx
    pop bx
    pop ax

    ret
    

;; Stores the given register's value as a hex string
;; at the given location. (NULL terminator is NOT
;; included)
;; Writes 4 characters always.
;;
;; Params:
;;  di: the desination to store at.
;;  ax: the register to translate.
;;store_hex_str:
;;    push cx
;;    push bx
;;    push di
;;
;;    ;; ax contains 16 bits (4 groups of 4)    
;;    ;; each group of 4 must be translated.
;;    
;;    ;; we are going to do 4 iterations.
;;    mov cl, 4
;;
;;.loop:
;;    mov bx, ax
;;
;;    sar bx, 12  ;; Will copy high bit.
;;    and bx, 0x000F
;;
;;    cmp bx, 10
;;    jge .hex_digit
;;
;;.decimal_digit: 
;;    add bl, "0"
;;    jmp .continue
;;
;;.hex_digit:
;;    add bl, "A" - 10
;;
;;.continue:
;;    mov [di], bl
;;    inc di
;;
;;    rol ax, 4
;;
;;    dec cl
;;    jz .exit
;;    jmp .loop
;;
;;.exit:
;;    pop di
;;    pop bx
;;    pop cx
;;     
;;    ret

;; Print decimal value stored in ax.
;; Params:
;;  ax: value to print.
print_dec_str:
    push di
    push si

    mov di, .buf
    call store_dec_str
    mov si, .buf
    call print_str

    pop si
    pop di
    
    ret

.buf: db 0, 0, 0, 0, 0, 0

;; Print hex value stored in ax.
;; Params:
;;  ax: value to print.
;;print_hex_str:
;;    push di
;;    push si
;;
;;    mov di, .buf
;;    call store_hex_str
;;    mov si, .buf
;;    call print_str
;;
;;    pop si
;;    pop di
;;    
;;    ret
;;
;;.buf: db 0, 0, 0, 0, 0

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

print_newline:
    push si

    mov si, newline
    call print_str

    pop si
    ret

newline: db ENDL, 0

bytes_left: dw 510-($-$$)

times 510-($-$$) db 0

;; Boot signature.
dw 0xAA55
