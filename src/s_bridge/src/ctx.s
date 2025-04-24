

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
    movw %ds, %ax
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
    // Skip return value
    add $4, %esp

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

.global nop_master_irq_handler
nop_master_irq_handler:
    pushl $0 // nop error code.
    call enter_intr_ctx
    call pic_send_master_eoi
    call exit_intr_ctx

.global nop_master_irq7_handler
nop_master_irq7_handler:
    pushl $0 // nop error code.
    call enter_intr_ctx
    call _nop_master_irq7_handler
    call exit_intr_ctx

.global nop_slave_irq_handler
nop_slave_irq_handler:
    pushl $0 // nop error code.
    call enter_intr_ctx
    call pic_send_slave_eoi
    call pic_send_master_eoi
    call exit_intr_ctx

.global nop_slave_irq15_handler
nop_slave_irq15_handler:
    pushl $0 // nop error code.
    call enter_intr_ctx
    call _nop_slave_irq15_handler
    call exit_intr_ctx

.global return_to_ctx
return_to_ctx:
    movl 4(%esp), %edi // Store the context address.

    subl $(17 * 4), %esp // reserve room for the context.
    movl %esp, %esi     // Store the top of context.

    movl %esp, %ebp
    pushl $(17 * 4)  // n
    pushl %edi       // src
    pushl %esi       // dest
    call mem_cpy
    movl %ebp, %esp

    // One last thing, we need to modify the temp_esp value in the context
    // so that popal works correctly.
    movl %esp, %esi
    addl $(10 * 4), %esi // index of `uint32_t reserved`

    movl %esp, %edi
    addl $(5 * 4), %edi // index of `uint32_t temp_esp`

    movl %esi, (%edi)

    call exit_intr_ctx

.global gpf_handler
gpf_handler:
    // No need to push 0 here, gpf has a build in error code.
    call enter_intr_ctx
    pushl %esp
    call *gpf_action

.global timer_handler
timer_handler:
    pushl $0 // Timer has no error.
    call enter_intr_ctx
    call pic_send_master_eoi

    pushl %esp
    call *timer_action

.global trigger_syscall
trigger_syscall:
    movl 8(%esp), %ecx // arg
    movl 4(%esp), %eax // id

    int $48
    
    // This syscall should return right here with %eax loaded
    // with the correct return value.

    ret

.global syscall_handler
syscall_handler:
    pushl $0 // Noop error code.
    call enter_intr_ctx
    
    movl 8 * 4(%esp), %ecx // Arg
    movl 9 * 4(%esp), %eax // ID
    movl %esp, %esi        // Context pointer

    pushl %ecx
    pushl %eax
    pushl %esi

    call *syscall_action
