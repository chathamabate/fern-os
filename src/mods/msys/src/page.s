

.section .text

.global load_page_directory
.type load_page_directory, @function
load_page_directory:
    push %ebp
    movl %esp, %ebp

    movl 8(%ebp), %eax
    movl %eax, %cr3

    movl %ebp, %esp
    pop %ebp
    ret

.global enable_paging
.type enable_paging, @function
enable_paging:
    push %ebp
    movl %esp, %ebp

    movl %cr0, %eax
    or $0x80000000, %eax
    movl %eax, %cr0

    movl %ebp, %esp
    pop %ebp
    ret
