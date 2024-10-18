

.section .text

.global read_esp
.type reap_esp, @function
read_esp:
    leal 4(%esp), %eax
    ret
