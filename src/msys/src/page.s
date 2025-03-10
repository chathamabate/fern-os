

.global enable_paging
enable_paging:
    movl %cr0, %eax
    orl $0x80000001, %eax
    movl %eax, %cr0
    ret

.global get_page_directory
get_page_directory:
    movl %cr3, %eax
    ret

.global set_page_directory
set_page_directory:
    pushl %ebp
    movl %esp, %ebp

    movl 8(%ebp), %eax
    movl %eax, %cr3

    movl %ebp, %esp
    popl %ebp

    ret

