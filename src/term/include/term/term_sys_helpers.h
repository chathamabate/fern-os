
#pragma once

#include "msys/dt.h"
#include "msys/idt.h"
#include "msys/gdt.h"
#include "msys/page.h"

void term_put_seg_selector(seg_selector_t ssr);
void term_put_seg_desc(seg_desc_t sd);
void term_put_gate_desc(gate_desc_t gd);

void term_put_dtr(dtr_val_t dtv);

void term_put_pte(pt_entry_t pte);


