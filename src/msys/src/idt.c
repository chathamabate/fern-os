
#include "msys/dt.h"
#include "msys/idt.h"
#include "msys/intr.h"

extern void _load_idtr(dtr_val_t idtv);

void load_idtr(dtr_val_t idtv) {
    uint32_t en = intr_section_enter();
    _load_idtr(idtv);
    intr_section_exit(en);
}
