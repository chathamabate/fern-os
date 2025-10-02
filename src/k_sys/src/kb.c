
#include "k_sys/kb.h"
#include "s_util/misc.h"

static fernos_error_t expect_ack(uint8_t cmd) {
    uint8_t byte;

    i8042_wait_for_empty_input_buffer();
    outb(I8042_W_INPUT_PORT, cmd);
    i8042_wait_for_full_output_buffer();
    byte = inb(I8042_R_OUTPUT_PORT);

    if (byte != PS2_ACK) {
        return FOS_E_STATE_MISMATCH;
    }

    return FOS_E_SUCCESS;
}

fernos_error_t init_kb(void) {
    uint8_t byte;

    // Alright, this is kinda overkill, but we'll just read out all data which
    // may be in the output buffer.
    while (inb(I8042_R_STATUS_REG_PORT) & I8042_STATUS_REG_OBF) {
        inb(I8042_R_OUTPUT_PORT);
    }

    /*
     * Really the checks below themselves are kinda overkill, I kinda just 
     * thought them up based on how I'd intuitively expect things to work.
     *
     * Really, the default from the BIOS should be good enough, but whatever.
     */

    // Next, let's run a self-test on the controller.
    outb(I8042_W_SEND_CMD_PORT, I8042_CMD_CONTROLLER_SELF_TEST);

    i8042_wait_for_full_output_buffer();
    byte = inb(I8042_R_OUTPUT_PORT);

    // We expect this byte back from the controller self-test.
    if (byte != 0x55) {
        return FOS_E_UNKNWON_ERROR;
    }

    // Next let's read the current command byte.
    outb(I8042_W_SEND_CMD_PORT, I8042_CMD_READ_CMD_BYTE);

    i8042_wait_for_full_output_buffer();
    byte = inb(I8042_R_OUTPUT_PORT);

    // Make sure interrupts and scancode set 1 translation are on.
    byte |= I8042_CMDBYTE_INT | I8042_CMDBYTE_XLAT; 

    outb(I8042_W_SEND_CMD_PORT, I8042_CMD_WRITE_CMD_BYTE);
    outb(I8042_W_INPUT_PORT, byte);

    // Cool, now let's enable the keyboard interface.
    outb(I8042_W_SEND_CMD_PORT, I8042_CMD_ENABLE_KB_IF);

    // Set PS/2 to defaults.
    PROP_ERR(expect_ack(PS2_CMD_SET_DEFAULT));
    PROP_ERR(expect_ack(PS2_CMD_ENABLE));

    // Finally run a self test on on the KB interface from the 8042.

    outb(I8042_W_SEND_CMD_PORT, I8042_CMD_KB_IF_SELF_TEST);

    i8042_wait_for_full_output_buffer();
    byte = inb(I8042_R_OUTPUT_PORT);

    if (byte != 0x00) {
        return FOS_E_UNKNWON_ERROR;
    }

    return FOS_E_SUCCESS;
}
