
.section .text
    
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

    movl %cr3, %ebx  // old pd.
    movl %esp, %edx  // old esp.

    movl intr_pd, %edi
    movl %edi, %cr3
    movl intr_esp, %esp

    pushl %edx
    pushl %ebx

    // Notify the pic. (Must be done from kernel space)
    call pic_send_master_eoi

    call *timer_action

.global random_handler
random_handler:
    
    pushl $4
    pushl %esp
    pushl $1
    call term_put_trace
    call lock_up

aaa: .ascii "Hello from random handler\n\0"
