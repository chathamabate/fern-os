
#include "u_startup/main.h"

#include "u_startup/syscall_gfx.h"
#include "u_startup/syscall.h"
#include "u_startup/test/syscall_gfx.h"
#include "s_mem/simple_heap.h"
#include "s_mem/test/simple_heap.h"
#include "u_startup/test/syscall.h"
#include "u_startup/test/syscall_fs.h"
#include "s_data/test/binary_search_tree.h"

proc_exit_status_t user_main(void) {
    /*
     * User Code Here.
     */

    // Ok, potential directions:
    //      * Better userspace terminal?
    //      * Userspace graphics window infra?
    //      * Reworked testing? (Could be fun and useful)
    //      I mean some sort of way of running many tests in a row?
    //      IDK, this is kinda difficult in this env tbh?
    //      Certain tests can only be run at certain times anyway?
    //      I guess it'd be cool to have a better place for some of this shit?
    //      IDK, I feel like configurations aren't that bad at the moment?
    //      IDK, I mean, it's not great...
    //      I guess what I am trying to say is that it would be cool if we could store multiple
    //      multiple configurations in different places??? Right... we can keep constraints in C
    //      file right???
    //      I mean, it doesn't really need to be that complex imo.
    //      We can still leave errors inside the C code imo.
    //
    //      Configs can live as JSON
    //      JSON can be converted -> C header, LD Header, and ASM?
    //      Not the end of the world?
    //      I mean, would this really be worth it??
    //      I think so..... I mean, most of all it would be fun!
    //      We may want to consider doing ISO Loading first though???
    //      Ultimately, we are trying to produce a single image right??
    //      Well, that's not totally true, we may want to produce many different things...
    //      IDK, I don't really event want to do anything right now, plus it's raining.
    //      Should we build a fernconf tool???
    //      I mean, that could be kinda cool tbh... Why even put it in this repo?
    //      Ok, but won't constraints be specified in python???
    //      What if we want quick changes and shit??
    //      Git submodules aren't so bad IMO? But I think we'd want pip for that tbh.
    //      Yes, this is also true...
    //

    handle_t h;
    sc_gfx_new_terminal(&h, NULL);
    sc_set_out_handle(h);

    test_syscall();

    // I think this guy requires a heap?
    // I mean, we could make this work a little bit?
    // could be a fun 
    // Maybe the possibility of running many tests?
    // Like maybe the userspace tests could be better?
    // IDK, just throwing that out there?
    test_syscall_fs();

    test_syscall_gfx();
    while (1);

    return PROC_ES_SUCCESS;
}
