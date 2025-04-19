
.section .text

.global enter_intr_ctx
enter_intr_ctx:
    // We must move our return address to the new stack.
    popl %edx

    movl %cr3, %eax
    movl intr_pd, %ecx
    cmpl %eax, %ecx
    jne 2f

1: // Equal Case
    movl %esp, %eax // Save current stack.
    pushl intr_pd   
    pushl %eax
    jmp 3f

2: // Not Equal Case
    movl %cr3, %eax
    movl %esp, %ecx

    // Kinda hacky, but I'm using esp as a general purpose register here real quick.
    movl intr_pd, %esp
    movl %esp, %cr3

    movl intr_esp, %esp

    pushl %eax // Save old context.
    pushl %ebx

    jmp 3f

3:
    pushl %edx // Push return address onto the stack and return.
    ret

.global leave_intr_ctx
leave_intr_ctx:
    popl %edx // Save return address.

    popl %eax // Old ESP
    popl %ecx // Old cr3

    movl %ecx, %cr3
    movl %eax, %esp

    pushl %edx
    ret
    
.global lock_up_handler
lock_up_handler:
    movl intr_esp, %esp
    movl intr_pd, %eax
    movl %eax, %cr3

    call _lock_up_handler // NEVER RETURNS!

    


.global nop_master_irq_handler
nop_master_irq_handler:
    pushal
    call enter_intr_ctx
    call pic_send_master_eoi
    call leave_intr_ctx
    popal
    iret

.global nop_master_irq7_handler
nop_master_irq7_handler:
    pushal
    call enter_intr_ctx
    call _nop_master_irq7_handler
    call leave_intr_ctx
    popal
    iret

.global nop_slave_irq_handler
nop_slave_irq_handler:
    pushal
    call enter_intr_ctx
    call pic_send_slave_eoi
    call pic_send_master_eoi
    call leave_intr_ctx
    popal
    iret

.global nop_slave_irq15_handler
nop_slave_irq15_handler:
    pushal
    call enter_intr_ctx
    call _nop_slave_irq15_handler
    call leave_intr_ctx
    popal
    iret

.global timer_handler
timer_handler:
    pushal
    call enter_intr_ctx

    pushl $msg
    call term_put_s
    popl %eax 

    call pic_send_master_eoi

    call leave_intr_ctx
    popal
    iret
msg: .ascii "Hello from timer\n\0"
