
;; IO functions!

%define ENDL 0x0D, 0x0A

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

tui_init:
    ;; Code for disabling cursor.
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
    push ds

    mov ax, TUI_BUFFER_SEG
    mov ds, ax

    mov bx, 0
.loop:
    mov byte [bx],     " "
    mov byte [bx + 1], COLOR(WHITE, BLACK)

    add bx, 2
    cmp bx, (2*TUI_BUFFER_LEN)
    jne .loop

    pop ds

    ret



;; Place a string in the BIOS video memory.
;; Params:
;;  si: Address of the beginning of the string.
;;   
tui_puts:


