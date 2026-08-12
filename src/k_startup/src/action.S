
#include "s_config.h"

.section .text

.global return_to_halt_context
return_to_halt_context:
	movl $(FC_CORE_VMEM_STACK_END-4), %esp
    movl %esp, %ebp
    call _return_to_halt_context
    
