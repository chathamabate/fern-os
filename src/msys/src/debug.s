

.section .text

.global read_eflags
read_eflags:
    pushf
    popl %eax
    ret

.global read_esp
read_esp:
    leal 4(%esp), %eax
    ret

.global read_cs
read_cs:
    movl $0, %eax
    movw %cs, %ax
    ret

.global read_ds
read_ds:
    movl $0, %eax
    movw %ds, %ax
    ret

.global lock_up
lock_up:
    cli
.loop:
    hlt 
    jmp .loop
