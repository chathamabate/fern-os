
#include "msys/dt.h"
#include "msys/intr.h"

void load_gate_descriptor(size_t i, gate_descriptor_t gd) {
    idtr_val_t idtr;
    read_idtr(&idtr);
    
    disable_intrs();
    idtr.offset[i] = gd;
    enable_intrs();
}
