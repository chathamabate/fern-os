
#include "msys/gdt.h"
#include "msys/intr.h"

// Load the gdtr register without worrying about interrupt toggling.
extern void _load_gdtr(gdtr_val_t v);

void load_gdtr(gdtr_val_t v) {
    uint32_t en = intr_section_enter();
    _load_gdtr(v);
    intr_section_exit(en);
}
