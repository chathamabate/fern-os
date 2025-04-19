
.section .text

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
