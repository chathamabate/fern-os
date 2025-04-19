
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
    pushl %ecx

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

.global syscall_enter_handler
syscall_enter_handler:
    pushal

    // %eax - id
    // %ecx - arg
    movl %cr3, %ebx  // old pd.
    movl %esp, %edx  // old esp.
    
    movl intr_pd, %edi
    movl %edi, %cr3
    movl intr_esp, %esp

    pushl %ecx
    pushl %eax
    pushl %edx
    pushl %ebx

    call *syscall_action // SHOULD NOT RETURN DIRECTLY!

.global context_return
context_return:
    movl 4(%esp), %eax // The PD
    movl 8(%esp), %ebx // The stack pointer.

    movl %eax, %cr3
    movl %ebx, %esp

    popal
    iret

.global context_return_value
context_return_value:
    movl 4(%esp), %eax // The PD
    movl 8(%esp), %ebx // The stack pointer.
    movl 12(%esp), %ecx // The return value.

    movl %eax, %cr3
    movl %ebx, %esp

    movl %ecx, 28(%esp) // Write in return value!

    popal
    iret

.global trigger_syscall
trigger_syscall:
    movl 4(%esp), %eax
    movl 8(%esp), %ecx
    int $48

    // When the kernel returns here, %eax should be loaded with
    // the appropriate return value.

    ret


.global timer_handler
timer_handler:
    pushal
    call enter_intr_ctx

    /*
    pushl shared_val
    pushl $_fmt_str
    call term_put_fmt_s
    popl %eax
    popl %eax
    */
    /*
    pushl $msg
    call term_put_s
    popl %eax 
    */

    call pic_send_master_eoi

    call leave_intr_ctx
    popal
    iret
_fmt_str: .ascii "Shared Val: %u\n\0"
