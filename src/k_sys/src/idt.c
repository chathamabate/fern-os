
#include "k_sys/dt.h"
#include "k_sys/idt.h"
#include "k_sys/intr.h"

extern void _load_idtr(dtr_val_t idtv);

void load_idtr(dtr_val_t idtv) {
    uint32_t en = intr_section_enter();
    _load_idtr(idtv);
    intr_section_exit(en);
}

gate_desc_t get_gd(uint8_t i) {
    dtr_val_t idtv = read_idtr();

    uint32_t num_entries = dtv_get_num_entries(idtv);
    if (i >= num_entries) {
        return not_present_gate_desc();
    }

    gate_desc_t *idt = dtv_get_base(idtv);
    return idt[i];
}

void set_gd(uint8_t i, gate_desc_t gd) {
    uint32_t en = intr_section_enter();

    dtr_val_t idtv = read_idtr();

    uint32_t num_entries = dtv_get_num_entries(idtv);
    if (i >= num_entries) {
        return;
    }

    gate_desc_t *idt = dtv_get_base(idtv);
    idt[i] = gd;

    intr_section_exit(en);
}
