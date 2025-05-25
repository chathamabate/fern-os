.section .text
.globl mem_cmp
.globl mem_cpy
.globl mem_chk
.globl mem_set
.globl str_eq
.globl str_cpy
.globl str_len

mem_cmp:
    pushl %ebp
    movl %esp, %ebp

    movl 8(%ebp), %esi    # d0
    movl 12(%ebp), %edi   # d1
    movl 16(%ebp), %ecx   # n

    testl %ecx, %ecx
    je .cmp_equal

.cmp_loop:
    movb (%esi), %al
    movb (%edi), %bl
    cmpb %bl, %al
    jne .cmp_not_equal
    incl %esi
    incl %edi
    decl %ecx
    jnz .cmp_loop

.cmp_equal:
    movl $1, %eax
    popl %ebp
    ret

.cmp_not_equal:
    movl $0, %eax
    popl %ebp
    ret

mem_cpy:
    pushl %ebp
    movl %esp, %ebp

    movl 8(%ebp), %edi    # dest
    movl 12(%ebp), %esi   # src
    movl 16(%ebp), %ecx   # n

    cld
    rep movsb

    popl %ebp
    ret

mem_chk:
    pushl %ebp
    movl %esp, %ebp

    movl 8(%ebp), %esi     # src
    movb 12(%ebp), %al     # val
    movl 16(%ebp), %ecx    # n

    testl %ecx, %ecx
    je .chk_equal

.chk_loop:
    movb (%esi), %bl
    cmpb %al, %bl
    jne .chk_not_equal
    incl %esi
    decl %ecx
    jnz .chk_loop

.chk_equal:
    movl $1, %eax
    popl %ebp
    ret

.chk_not_equal:
    movl $0, %eax
    popl %ebp
    ret

mem_set:
    pushl %ebp
    movl %esp, %ebp

    movl 8(%ebp), %edi    # dest
    movb 12(%ebp), %al    # val
    movl 16(%ebp), %ecx   # count

    cld
    rep stosb

    popl %ebp
    ret

str_eq:
    pushl %ebp
    movl %esp, %ebp

    movl 8(%ebp), %esi      # s1
    movl 12(%ebp), %edi     # s2

.str_eq_loop:
    movb (%esi), %al
    movb (%edi), %bl
    cmpb %al, %bl
    jne .str_eq_false       # if s1[i] != s2[i]

    cmpb $0, %al
    je .str_eq_true         # if s1[i] == '\0', then equal

    incl %esi
    incl %edi
    jmp .str_eq_loop

.str_eq_true:
    movl $1, %eax
    popl %ebp
    ret

.str_eq_false:
    movl $0, %eax
    popl %ebp
    ret

str_cpy:
    pushl %ebp
    movl %esp, %ebp

    movl 8(%ebp), %edi      # dest
    movl 12(%ebp), %esi     # src
    xor %ecx, %ecx          # i = 0

.str_cpy_loop:
    movb (%esi, %ecx, 1), %al
    movb %al, (%edi, %ecx, 1)
    cmpb $0, %al
    je .str_cpy_done
    incl %ecx
    jmp .str_cpy_loop

.str_cpy_done:
    movl %ecx, %eax         # return i
    popl %ebp
    ret

str_len:
    pushl %ebp
    movl %esp, %ebp

    movl 8(%ebp), %esi      # src
    xor %ecx, %ecx          # i = 0

.str_len_loop:
    movb (%esi, %ecx, 1), %al
    cmpb $0, %al
    je .str_len_done
    incl %ecx
    jmp .str_len_loop

.str_len_done:
    movl %ecx, %eax
    popl %ebp
    ret
