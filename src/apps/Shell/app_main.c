

#include "u_startup/syscall.h"
#include "s_bridge/shared_defs.h"
#include "s_util/constraints.h"
#include "s_util/ansi.h"
#include <stddef.h>
#include "s_util/ps2_scancodes.h"


/*
 * NOTE: Probably going to come back to this after graphics!
 */

/*
 * NOTE: The Shell binary expects key codes on default in and a character display on default out.
 * The shell will fail if default out is not a character display.
 * 
 * I think there need to be some special characters in place?
 * <ARG> := Hmmmm, I don't really even know the best way to do this?
 * <CMD> := 
 */


#define CMD_BUF_LEN (512U)

/**
 * Will make this more dynamic in the future.
 */
#define PROMPT ANSI_BRIGHT_GREEN_FG "> " ANSI_RESET

static void single_prompt(void);
static void run_cmd(const char *cmd);

proc_exit_status_t app_main(const char * const *args, size_t num_args) {
    // Probably will redo all of this tbh...
    // Maybe not right now though...
    handle_t h = sc_get_out_handle();
    if (h == FOS_MAX_HANDLES_PER_PROC || !sc_handle_is_terminal(h)) {
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
            if (scs1_is_make(code)) {
                cmd_buf[cmd_pos] = '\0';
                sc_out_write_s("\n");
                run_cmd(cmd_buf);
                return;
            }
            break;

        case SCS1_BACKSPACE:
            if (scs1_is_make(code) && cmd_pos > 0) {
                sc_out_write_s("\b \b"); 
                cmd_buf[--cmd_pos] = '\0';
            }
            break;

        default: {
            char c = lshift_pressed || rshift_pressed 
                ? scs1_to_ascii_uc(make_code) : scs1_to_ascii_lc(make_code);

            if (scs1_is_make(code) && c) {
                cmd_buf[cmd_pos++] = c;
                sc_out_write_full(&c, sizeof(c));

                if (cmd_pos == CMD_BUF_LEN) {
                    cmd_buf[CMD_BUF_LEN] = '\0';
                    sc_out_write_s("\n");
                    run_cmd(cmd_buf);
                    return;
                }
            }
        }

        }
    }
}

static void run_cmd(const char *cmd) {
    if (cmd[0] == '\0') {
        return; // Do nothing for an empty command!
    }

    sc_out_write_s(cmd);
    sc_out_write_s("\n");
}
