
.section .text

.global flush_tss
flush_tss:
    movl 4(%esp), %eax
    ltr %ax
    ret
