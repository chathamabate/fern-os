

.section .text

.global read_esp
.type reap_esp, @function
read_esp:
    leal 4(%esp), %eax
    ret

.global read_cs
.type reap_cs, @function
read_cs:
    movl $0, %eax
    movw %cs, %ax
    ret

.global read_ds
.type reap_ds, @function
read_ds:
    movl $0, %eax
    movw %ds, %ax
    ret
