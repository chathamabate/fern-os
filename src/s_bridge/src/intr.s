
.section .text

.global lock_up_handler
lock_up_handler:
    movl intr_esp, %esp
    movl intr_pd, %eax
    movl %eax, %cr3

    // I'll be damned, chatgpt was right about this one.
    call *lock_up_action
    // SHOULD NEVER RETURN!
