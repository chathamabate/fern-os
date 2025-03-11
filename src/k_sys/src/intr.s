
.section .text

.global enable_intrs
enable_intrs:
    sti
    ret

.global disable_intrs
disable_intrs:
    cli
    ret

.global nop_handler
nop_handler:
    iret

.global nop_master_irq_handler
nop_master_irq_handler:
    pushal
    call pic_send_master_eoi
    popal
    iret

.global nop_master_irq7_handler
nop_master_irq7_handler:
    pushal
    call _nop_master_irq7_handler
    popal
    iret

.global nop_slave_irq_handler
nop_slave_irq_handler:
    pushal
    call pic_send_slave_eoi
    call pic_send_master_eoi
    popal
    iret

.global nop_slave_irq15_handler
nop_slave_irq15_handler:
    pushal
    call _nop_slave_irq15_handler
    popal
    iret

