;; Terminal functions!

%define BLACK       0x0
%define BLUE        0x1
%define GREEN       0x2
%define CYAN        0x3
%define RED         0x4
%define MAGENTA     0x5
%define BROWN       0x6
%define LIGHT_GREY  0x7

%define DARK_GREY       0x8
%define LIGHT_BLUE      0x9
%define LIGHT_GREEN     0xA
%define LIGHT_CYAN      0xB
%define LIGHT_RED       0xC
%define LIGHT_MAGENTA   0xD
%define YELLOW          0xE
%define WHITE           0xF

%define COLOR(fg, bg) (((bg) << 4) | (fg))

%define TUI_COLS 80
%define TUI_ROWS 25

;; NOTE: each cell is 2 bytes!
%define TUI_BUFFER_LEN ((TUI_COLS) * (TUI_ROWS))

%define TUI_BUFFER_SEG 0xB800

;; Linear Text Buffer.
;; 0xB8000 (80 x 25) characters.

;; tui works with the BIOS vga3 text mode display!


;; Initialize the tui.
;; Params:
;;  None
tui_init:
    mov dx, 0x3D4
	mov al, 0xA
	out dx, al
 
	inc dx
	mov al, 0x20	
	out dx, al

    call tui_clear

    ret

;; Clear the linear buffer.
tui_clear:
    mov ax, TUI_BUFFER_SEG
    mov ds, ax

    mov bx, 0
.loop:
    cmp bx, (2*TUI_BUFFER_LEN)
    jnl .end

    mov byte [bx],     " "
    mov byte [bx + 1], COLOR(WHITE, BLACK)

    add bx, 2
    jmp .loop

.end:
    ret

;; Place a string in the BIOS video memory.
;; Params:
;;  [bp-1]: u8 row
;;  [bp-2]: u8 col
;;  [bp-3]: u8 color
;;  [bp-5]: u16 source segment
;;  [bp-7]: u16 source offset
tui_puts:
    ;; Reading source string from ds:si.
    mov ds, [bp-5]
    mov si, [bp-7]

    ;; We exit if we hit the end of the string,
    ;; or we hit the end of the buffer.

    mov ah, 0
    mov al, [bp-1] 

    mov bl, TUI_COLS
    mul bl 

    add al, [bp-2]
    adc ah, 0
    
    mov di, ax
    sal di, 1

    ;; Copying to es:di.
    mov ax, TUI_BUFFER_SEG
    mov es, ax

    ;; Retrieve our color.
    mov ch, [bp-3]
.loop:
    cmp di, (2*TUI_BUFFER_LEN)
    jnl .end

    mov cl, [ds:si]
    
    ;; Check for end of string.
    cmp cl, 0
    je .end

    ;; Finally perform write.
    mov [es:di], cx
    
    inc si      ;; move in source.
    add di, 2   ;; move in dest.
    
    jmp .loop

.end:
    ret
