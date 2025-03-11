
#include "k_sys/dt.h"
#include "k_sys/gdt.h"
#include "k_sys/intr.h"

// Load the gdtr register without worrying about interrupt toggling.
extern void _load_gdtr(dtr_val_t v);

void load_gdtr(dtr_val_t v) {
    uint32_t en = intr_section_enter();
    _load_gdtr(v);
    intr_section_exit(en);
}
