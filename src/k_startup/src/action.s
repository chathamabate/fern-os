
.section .text

.include "os_defs.s"

.global return_to_halt_context
return_to_halt_context:
	movl $(FERNOS_END+1-4), %esp
    movl %esp, %ebp
    call _return_to_halt_context
    
