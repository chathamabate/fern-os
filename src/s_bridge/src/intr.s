
.section .text

.global enter_intr_ctx
enter_intr_ctx:

    movl %esp, %eax
    movl %cr3, %ecx

    movl intr_pd, %edx
    cmpl %ecx, %edx
    jne 2f

1: // Equal Case
    
2: // Not Equal Case
3:

    // Write out current page directory.
    movl 4(%esp), %eax
    movl %cr3, %ecx
    movl %ecx, (%eax)

    // Write out stack location to return to.
    movl 8(%esp), %eax
    movl %esp, %ecx
    movl %ecx, (%eax)

    popl %edx // Since we will be switching stacks, let's save our return
              // address for later.

    // We need to compare the current page directory with the interrupt context
    // page directory.
    movl %cr3, %eax
    movl intr_pd, %ecx
    cmpl %eax, %ecx
    jne 2f

1: // Not equal Case.

    // Go to the interrupt context location!
    movl intr_pd, %eax
    movl %eax, %cr3
    movl intr_esp, %esp

    jmp 3f

2:  // Equal case.
    
    // Do we even do anything here??

3:
    


.global lock_up_handler
lock_up_handler:
    movl intr_esp, %esp
    movl intr_pd, %eax
    movl %eax, %cr3

    call _lock_up_handler // NEVER RETURNS!

    


    /*
.global nop_master_irq_handler
nop_master_irq_handler:
    pushal 

    // Push current page directory.
    //pushl %cr3

    // Load kernel page directory.
    movl intr_esp, %esp
    movl intr_pd, %eax
    movl %eax, %cr3

    call pic_send_master_eoi

    // Go back to other page directory.
    //popl %cr3
    
    popal
    iret

.global nop_master_irq7_handler
nop_master_irq7_handler:
    iret

.global nop_slave_irq_handler
nop_slave_irq_handler:
    iret

.global nop_slave_irq15_handler
nop_slave_irq15_handler:
    iret
    */
