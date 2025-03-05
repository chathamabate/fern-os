

.section .text

.global read_esp
.type read_esp, @function
read_esp:
    leal 4(%esp), %eax
    ret

.global read_cs
.type read_cs, @function
read_cs:
    movl $0, %eax
    movw %cs, %ax
    ret

.global read_ds
.type read_ds, @function
read_ds:
    movl $0, %eax
    movw %ds, %ax
    ret

.global lock_up
.type lock_up, @function
lock_up:
    cli
.loop:
    hlt 
    jmp .loop
