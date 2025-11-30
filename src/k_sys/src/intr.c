
#include "k_sys/intr.h"

// Ok, now some PIC stuff..... (Much of this taken from OSdev Wiki

#define PIC1		0x20		/* IO base address for master PIC */
#define PIC2		0xA0		/* IO base address for slave PIC */
#define PIC1_COMMAND	PIC1
#define PIC1_DATA	(PIC1+1)
#define PIC2_COMMAND	PIC2
#define PIC2_DATA	(PIC2+1)
#define PIC_EOI		0x20		/* End-of-interrupt command code */

#define ICW1_ICW4	0x01		/* Indicates that ICW4 will be present */
#define ICW1_SINGLE	0x02		/* Single (cascade) mode */
#define ICW1_INTERVAL4	0x04	/* Call address interval 4 (8) */
#define ICW1_LEVEL	0x08		/* Level triggered (edge) mode */
#define ICW1_INIT	0x10		/* Initialization - required! */

#define ICW4_8086	0x01		/* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO	0x02		/* Auto (normal) EOI */
#define ICW4_BUF_SLAVE	0x08	/* Buffered mode/slave */
#define ICW4_BUF_MASTER	0x0C	/* Buffered mode/master */
#define ICW4_SFNM	0x10		/* Special fully nested (not) */

void pic_remap(int offset1, int offset2) {
    // I am pretty sure that the "wait" nature of the below calls is necessary
    // when using in-line assembly. I believe that since my outb impl is an actual function,
    // the "wait" call isn't really needed as my outb is pretty slow. 
    //
    // Regardless, leaving it in for now.

	outb_and_wait(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);  // starts the initialization sequence (in cascade mode)
	outb_and_wait(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
	outb_and_wait(PIC1_DATA, offset1);                 // ICW2: Master PIC vector offset
	outb_and_wait(PIC2_DATA, offset2);                 // ICW2: Slave PIC vector offset
	outb_and_wait(PIC1_DATA, 4);                       // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
	outb_and_wait(PIC2_DATA, 2);                       // ICW3: tell Slave PIC its cascade identity (0000 0010)
	
	outb_and_wait(PIC1_DATA, ICW4_8086);               // ICW4: have the PICs use 8086 mode (and not 8080 mode)
	outb_and_wait(PIC2_DATA, ICW4_8086);

	// Unmask both PICs.
	outb(PIC1_DATA, 0);
	outb(PIC2_DATA, 0);
}

#define PIC_READ_IRR                0x0A    /* OCW3 irq ready next CMD read */
#define PIC_READ_ISR                0x0B    /* OCW3 irq service next CMD read */

static uint16_t pic_get_irq_reg(int ocw3)
{
    /* OCW3 to PIC CMD to get the register values.  PIC2 is chained, and
     * represents IRQs 8-15.  PIC1 is IRQs 0-7, with 2 being the chain */
    outb(PIC1_COMMAND, ocw3);
    outb(PIC2_COMMAND, ocw3);
    return (inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

/* Returns the combined value of the cascaded PICs irq request register */
uint16_t pic_get_irr(void)
{
    return pic_get_irq_reg(PIC_READ_IRR);
}

/* Returns the combined value of the cascaded PICs in-service register */
uint16_t pic_get_isr(void)
{
    return pic_get_irq_reg(PIC_READ_ISR);
}

void pic_mask_all(void) {
	outb(PIC1_DATA, 0xFF);
	outb(PIC2_DATA, 0xFF);
}

void pic_unmask_all(void) {
	outb(PIC1_DATA, 0x0);
	outb(PIC2_DATA, 0x0);
}

void pic_set_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if(irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) | (1 << irq);
    outb(port, value);        
}

void pic_clear_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if(irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) & ~(1 << irq);
    outb(port, value);        
}

void pic_send_master_eoi(void) {
    outb(PIC1_COMMAND,PIC_EOI);
}

void pic_send_slave_eoi(void) {
    outb(PIC2_COMMAND,PIC_EOI);
}

void pic_send_eoi(uint8_t irq) {
	if(irq >= 8)
		outb(PIC2_COMMAND,PIC_EOI);
	
	outb(PIC1_COMMAND,PIC_EOI);
}

void _nop_master_irq7_handler(void) {
    // Only send eoi if bit 7 was actually set to in-service.
    uint16_t isr = pic_get_isr();
    if (isr & (1 << 7)) {
        pic_send_master_eoi();        
    }
}

void _nop_slave_irq15_handler(void) {
    uint16_t isr = pic_get_isr();
    if (isr & (1 << 15)) {
        pic_send_slave_eoi();
    }

    // Always send master eoi
    pic_send_master_eoi();
}

/*
 * How the programmable interval timer works is actually kinda confusing, I'd recommend
 * looking at the OSDev wiki.
 *
 * The PIT has 3 channels, Channel 0 is what triggers IRQ0.
 *
 * The PIT receives a clock signal with rate ~1.19 MHz.
 * Using the square wave mode we can divide this frequency to a slower more practical rate.
 * In this mode, each channel has a "reload value". This is like a countdown to a state change.
 * (IRQ0 for channel 1).
 *
 * I believe with a reload value of 6, it'd look something like this.
 *
 * CLOCK:    ___–––___–––___–––___–––___–––___–––___–––___–––___–––
 * Counter:        6     4     2     6     4     2     6     4
 * OUT:            ––––––––––––––––––__________________––––––––––––
 *
 * So output would have a rising edge every six falling edges of the clock?
 * This diagram may be slightly shifted incorrectly, I'll learn more about this if I ever
 * dive into electronics.
 *
 * Anyway. To change a reload value just write to the a channel's data port!
 * To read a channels current count, write a latch count command to the command register
 * then read from the data port!
 */

#define PIT_CHNL_0_DATA_PORT (0x40U)
#define PIT_CHNL_1_DATA_PORT (0x41U)
#define PIT_CHNL_2_DATA_PORT (0x42U)
#define PIT_CMD_REG_PORT     (0x43U)

/*
 * PIT CMD bits [6:7] = Channel Select or Read Back!
 */

#define PIT_CMD_CHNL_0    (0x0U << 6)
#define PIT_CMD_CHNL_1    (0x1U << 6)
#define PIT_CMD_CHNL_2    (0x2U << 6)
#define PIT_CMD_READ_BACK (0x3U << 6)

/*
 * PIT CMD bits [4:5] = Access type or Latch command!
 */

#define PIT_CMD_LATCH_COUNT  (0x0U << 4)
#define PIT_CMD_LO_BYTE_ONLY (0x1U << 4)
#define PIT_CMD_HI_BYTE_ONLY (0x2U << 4)
#define PIT_CMD_LO_HI        (0x3U << 4)

/*
 * PIT CMD bits [1:3] = Operating mode
 */

#define PIT_CMD_SQUARE_WAVE_MODE (0x3U << 1)

void init_pit(uint16_t init_reload_val) {
    outb(PIT_CMD_REG_PORT, 
            PIT_CMD_CHNL_0 |
            PIT_CMD_LO_HI |
            PIT_CMD_SQUARE_WAVE_MODE);

    pit_set_reload_val(init_reload_val);
}

void pit_set_reload_val(uint16_t reload_val) {
    outb(PIT_CHNL_0_DATA_PORT, reload_val & 0xFF);
    outb(PIT_CHNL_0_DATA_PORT, (reload_val & 0xFF00) >> 8);
}

uint16_t pit_get_count(void) {
   // First let's latch the current count value. 
   outb(PIT_CMD_REG_PORT, 
           PIT_CMD_CHNL_0 | PIT_CMD_LATCH_COUNT);

   uint16_t count = 0;

   count |= inb(PIT_CHNL_0_DATA_PORT);
   count |= (inb(PIT_CHNL_0_DATA_PORT) << 8);

   return count;
}
