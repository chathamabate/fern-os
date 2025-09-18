
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
    PROP_ERR(expect_ack(PS2_CMD_SET_DEFAULT));
    PROP_ERR(expect_ack(PS2_CMD_ENABLE));

    outb(I8042_W_SEND_CMD_PORT, I8042_CMD_ENABLE_KEYBOARD);

    return FOS_E_SUCCESS;
}
