

#include "u_startup/syscall.h"
#include "s_bridge/shared_defs.h"
#include "s_util/constraints.h"
#include "s_util/ansi.h"
#include <stddef.h>
#include "s_util/ps2_scancodes.h"

/*
 * NOTE: The Shell binary expects keyboard on default in and a character display on default out.
 * The shell will fail if default out is not a character display.
 */

#define CMD_BUF_LEN (512U)

/**
 * Will make this more dynamic in the future.
 */
#define PROMPT ANSI_BRIGHT_GREEN_FG " > " ANSI_RESET

static void single_prompt(void);
static void run_cmd(const char *cmd);

proc_exit_status_t app_main(const char * const *args, size_t num_args) {
    handle_t h = sc_get_out_handle();
    if (h == FOS_MAX_HANDLES_PER_PROC || !sc_handle_is_cd(h)) {
        return PROC_ES_FAILURE;
    }

    // Do nothing with these for now.
    (void)args;
    (void)num_args;

    while (true) {
        single_prompt();
    }
}

static void single_prompt(void) {
    sc_out_write_s(PROMPT);

    size_t cmd_pos = 0;
    char cmd_buf[CMD_BUF_LEN + 1];

    bool lshift_pressed = false;
    bool rshift_pressed = false;

    while (true) {
        scs1_code_t code;
        sc_in_read_full(&code, sizeof(code));

        scs1_code_t make_code = scs1_as_make(code);
        switch (make_code) {
        case SCS1_LSHIFT:
            lshift_pressed = scs1_is_make(code);
            break;

        case SCS1_RSHIFT:
            rshift_pressed = scs1_is_make(code);
            break;

        case SCS1_ENTER:
            cmd_buf[cmd_pos] = '\0';
            run_cmd(cmd_buf);
            return;

        default: {
            char c = lshift_pressed || rshift_pressed 
                ? scs1_to_ascii_uc(make_code) : scs1_to_ascii_lc(make_code);

            if (c) {
                cmd_buf[cmd_pos++] = c;
                if (cmd_pos == CMD_BUF_LEN) {
                    cmd_buf[CMD_BUF_LEN] = '\0';
                    run_cmd(cmd_buf);
                    return;
                }
            }
        }

        }
    }
}

static void run_cmd(const char *cmd) {
    sc_out_write_s(cmd);
    sc_out_write_s("\n");
}
