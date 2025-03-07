
#pragma once

#include "msys/gdt.h"

void term_put_seg_desc(seg_desc_t sd);
void term_put_gdtv(gdtr_val_t gdtv);
