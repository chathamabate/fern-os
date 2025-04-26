

.section .text

.global thread_entry_routine
thread_entry_routine:
    pushl %ecx
    call *%eax

    // Here, %eax should be the return value from the
    // thread procedure.

    pushl %eax
    call sc_thread_exit

