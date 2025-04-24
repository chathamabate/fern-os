

.section .text

.include "os_defs.s"

/*
This is a Non-C style function. It expects to be called right after 
the interrupt handler is entered. It expects the very top of the stack
at the time of call to hold the error value pushed by the hardware.

If the interrupt being serviced does not push an error value, make sure
to push some dummy value anyway before calling this function!

After return, the interrupt context page table will be loaded (the kernel pd).
Also, the top of the stack will point to a valid user_ctx_t structure.
*/
.local enter_intr_ctx
enter_intr_ctx:
    // Save general purpose registers.
    pushal

    // Store the return address of this function.
    movl 8*4(%esp), %ecx

    // Save page table.
    movl %cr3, %eax
    pushl %eax

    // Save data segment.
    movl $0, %eax
    movw %ds, %eax
    pushl %eax 

    // Load kernel page table.
    movl FERNOS_END + 1 - 4, %eax
    movl %eax, %cr3

    // Switch to kernel data segments.
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    pushl %ecx
    ret

/*
Similar to `enter_intr_ctx` above, this function is not C-Like,
It expects the top of the stack holds a valid user_ctx_t.

It restores the context and calls iret!
*/
.local exit_intr_ctx
exit_intr_ctx:
    // Restore context data segment registers.
    popl %eax 
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    // Restore context page directory.
    popl %eax
    movl %eax, %cr3

    // Restore general purpose registers.
    popal

    // Skip over reserved and error code fields.
    addl $8, %esp

    // enter context!
    iret
    

.global lock_up_handler
lock_up_handler:
    movl intr_ctx_pd, %esi 
    movl (%esi), %eax
    movl %eax, %cr3

    // Fixed behavior to help with debugging.
    call _lock_up_handler 


.global nop_master_irq_handler
nop_master_irq_handler:
    pushal

    movl %cr3, %eax
    pushl %eax

    movl intr_ctx_pd, %esi 
    movl (%esi), %eax
    movl %eax, %cr3

    call pic_send_master_eoi

    popl %eax
    movl %eax, %cr3

    popal
    iret

.global nop_master_irq7_handler
nop_master_irq7_handler:
    pushal

    movl %cr3, %eax
    pushl %eax

    movl intr_ctx_pd, %esi 
    movl (%esi), %eax
    movl %eax, %cr3

    call _nop_master_irq7_handler

    popl %eax
    movl %eax, %cr3

    popal
    iret

.global nop_slave_irq_handler
nop_slave_irq_handler:
    pushal

    movl %cr3, %eax
    pushl %eax

    movl intr_ctx_pd, %esi 
    movl (%esi), %eax
    movl %eax, %cr3

    call pic_send_slave_eoi
    call pic_send_master_eoi

    popl %eax
    movl %eax, %cr3

    popal
    iret

.global nop_slave_irq15_handler
nop_slave_irq15_handler:
    pushal

    movl %cr3, %eax
    pushl %eax

    movl intr_ctx_pd, %esi 
    movl (%esi), %eax
    movl %eax, %cr3

    call _nop_slave_irq15_handler

    popl %eax
    movl %eax, %cr3

    popal
    iret

.global enter_user_ctx
enter_user_ctx:
    movl 4(%esp), %edi // Store the context address.

    subl $(15 * 4), %esp // reserve room for the context.
    movl %esp, %esi     // Store the top of context.

    movl %esp, %ebp
    pushl $(15 * 4)  // n
    pushl %edi       // src
    pushl %esi       // dest
    call mem_cpy
    movl %ebp, %esp

    // One last thing, we need to modify the temp_esp value in the context
    // so that popal works correctly.
    movl %esp, %esi
    addl $(9 * 4), %esi

    movl %esp, %edi
    addl $(4 * 4), %edi

    movl %esi, (%edi)

    // Load page directory.
    popl %eax
    movl %eax, %cr3 

    // Load general purpose registers.
    popal

    // Skip error value.
    addl $4, %esp

    // We assume that the context uses the same selector for data segments
    // and stack segments.
    movl 16(%esp), %eax

    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    // Finally RETURN!
    iret

.global timer_handler
timer_handler:
    pushl $0 // Timer has no error.

    pushal 

    // Switch to kernel data segment.
    xor %eax, %eax

    movw $0x18, %ax

    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    // Switch to interrupt context page table.
    movl %cr3, %eax
    pushl %eax

    movl intr_ctx_pd, %esi 
    movl (%esi), %eax
    movl %eax, %cr3

    call pic_send_master_eoi

    pushl %esp
    call *timer_action
