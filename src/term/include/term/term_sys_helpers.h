
#pragma once

#include "k_sys/dt.h"
#include "k_sys/idt.h"
#include "k_sys/gdt.h"
#include "k_sys/page.h"

void term_put_seg_selector(seg_selector_t ssr);
void term_put_seg_desc(seg_desc_t sd);
void term_put_gate_desc(gate_desc_t gd);

void term_put_dtr(dtr_val_t dtv);

void term_put_pte(pt_entry_t pte);


