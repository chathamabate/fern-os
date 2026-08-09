

.section .text

.global thread_entry_routine
thread_entry_routine:
    pushl %edx // arg2
    pushl %ecx // arg1
    pushl %ebx // arg0
    call *%eax

    // Here, %eax should be the return value from the
    // thread procedure.

    pushl %eax
    call sc_thread_exit

