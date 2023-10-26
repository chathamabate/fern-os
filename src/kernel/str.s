
    
;; Calc the hex string of a byte.
;; NOTE: this does not write a 
;; NULL character.
;;
;; NOTE: this ALWAYS writes 2 characters.
;;
;; Params:
;;  [bp-1]: u8 the byte.
;;  [bp-3]: u16 segment of buffer.
;;  [bp-5]: u16 offset of buffer.
str_u8_to_hex:
    mov ax, [bp-3]
    mov es, ax
    mov di, [bp-5]
    
    ;; Extract the byte.
    mov al, [bp-1]

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

;; Calc the hex string of a word.
;; NOTE: this ALWAYS writes 4 characters.
;;
;; Params:
;;  [bp-2]: u16 the word.
;;  [bp-4]: u16 segment of buffer.
;;  [bp-6]: u16 offset of buffer.
str_u16_to_hex:
    ;; We are going to use our above
    ;; call as a helper.
    ;;
    ;; Harder than we thought huh?
    ;;
    ;; How do we call one function from another
    ;; while not losing any information about bp??
    ;; This is a lil tricky my friend... a lil tricky...
    
    ;; Store are old bp in ax.
    ;; Gotta figure this out boys...
    ;; How will locals be accessed...
    ;; This is a good questione.
    mov ax, bp
    push bp

    mov bp, sp 
    sub sp, 5
    
    
    
    pop bp

    ret
    
    


