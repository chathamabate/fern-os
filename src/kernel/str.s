
    
;; Calc the hex string of a byte.
;; NOTE: this does not write a 
;; NULL character.
;;
;; NOTE: this ALWAYS writes 2 characters.
;;
;; Params:
;;  [bp-2]: u16 segment of buffer.
;;  [bp-4]: u16 offset of buffer.
;;  [bp-5]: u8 the byte.
str_u8_to_hex:
    mov ax, [bp-2]
    mov es, ax
    mov di, [bp-4]
    
    ;; Extract the byte.
    mov al, [bp-5]

    ;; Just gonna count to 2.
    mov cl, 0

.loop:
    cmp cl, 2
    jnl .end

    ror al, 4

    mov bl, al
    and bl, 0x0F

    cmp bl, 10 
    jl .dec

;; Hex case
    add bl, "A"-10
    jmp .continue

;; Decimal Case
.dec:
    add bl, "0"
    
.continue:

    mov [es:di], bl

    inc di
    inc cl

    jmp .loop

.end:
    ret

;; This converts a unsigned byte into its
;; 3 digit decimal form.
;; NOTE: this always writes 3 characters (No NT)
;;
;; Params:
;;  [bp-2]: u16 segment of buffer.
;;  [bp-4]: u16 offset of buffer.
;;  [bp-5]: u8 the byte.
str_u8_to_dec:
    ;; Load up ds:si to point to the buffer.
    mov ax, [bp-2]
    mov ds, ax
    mov si, [bp-4]

    ;; Write to the last character first.
    add si, 2

    ;; Load up the byte.
    mov al, [bp-5]

    
    mov bl, 10
    mov cl, 0

.loop:
    cmp cl, 3
    jge .end

    ;; Perform division.
    mov ah, 0
    div bl 

    ;; Write Character.
    add ah, "0"
    mov [si], ah
    dec si

    inc cl
    jmp .loop
.end:

    ret

;; This converts a signed byte into its
;; 3 digit decimal form.
;; NOTE: this always writes 4 characters (No NT)
;; (First character is the sign +/-)
;;
;; Params:
;;  [bp-2]: u16 segment of buffer.
;;  [bp-4]: u16 offset of buffer.
;;  [bp-5]: u8 the byte.
str_s8_to_dec:
    ;; Load up ds:si to point to the buffer.
    mov ax, [bp-2]
    mov ds, ax
    mov si, [bp-4]

    ;; Write to the last character first.
    add si, 2

    ;; Load up the byte.
    mov al, [bp-5]

    
    mov bl, 10
    mov cl, 0

.loop:
    cmp cl, 3
    jge .end

    ;; Perform division.
    mov ah, 0
    div bl 

    ;; Write Character.
    add ah, "0"
    mov [si], ah
    dec si

    inc cl
    jmp .loop
.end:

    ret
    

;; Calc the hex string of a word.
;; NOTE: this ALWAYS writes 4 characters (No NT)
;;
;; Params:
;;  [bp-2]: u16 segment of buffer.
;;  [bp-4]: u16 offset of buffer.
;;  [bp-6]: u16 the word.
str_u16_to_hex:
    ;; We are going to use our above
    ;; call as a helper.

    ;; Set up stack frame for first 
    ;; u8 call.
    push bp
    mov bp, sp  
    sub sp, 5

    ;; get our old base pointer.
    mov si, [bp]

    mov bx, [ss:si-2]
    mov [bp-2], bx
    
    mov bx, [ss:si-4]
    mov [bp-4], bx

    ;; load params.
    mov bl, [ss:si-5]
    mov [bp-5], bl

    call str_u8_to_hex

    ;; get our old base pointer. (Remember si might have changed)
    mov si, [bp]

    ;; load params.
    mov bl, [ss:si-6]
    mov [bp-5], bl
    add word [bp-4], 2

    call str_u8_to_hex

    ;; Revert to original stack config.
    mov sp, bp
    pop bp

    ret

;; Calc the hex string of a word.
;; NOTE: this ALWAYS writes 5 characters (No NT)
;;
;; Params:
;;  [bp-2]: u16 segment of buffer.
;;  [bp-4]: u16 offset of buffer.
;;  [bp-6]: u16 the word.
str_u16_to_dec:
    ;; Load up ds:si to point to the buffer.
    mov ax, [bp-2]
    mov ds, ax
    mov si, [bp-4]

    ;; Write to the last character first.
    add si, 4

    ;; Load up the byte.
    mov ax, [bp-6]

    mov bx, 10

    mov cl, 0

.loop:
    cmp cl, 5
    jge .end

    ;; Perform division.
    mov dx, 0
    div bx

    ;; Write Character.
    add dl, "0"
    mov [si], dl
    dec si

    inc cl
    jmp .loop
.end:

    ret
    


