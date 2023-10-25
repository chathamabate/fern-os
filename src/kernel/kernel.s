
org 0x7E00

bits 16

%define ENDL 0x0D, 0x0A

;; NOTE: from this point on, the boot sector will
;; be deprecated. DON'T USE IT'S FUNCTIONS!

start_kernel:
    mov si, kernel_msg
    call print_str

.halt
    hlt
    jmp .halt

kernel_msg: db "Hello From Kernel!", ENDL, 0

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
