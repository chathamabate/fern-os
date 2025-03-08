
#pragma once

#include "msys/dt.h"
#include "msys/gdt.h"

void term_put_seg_desc(seg_desc_t sd);

void term_put_gdtv(dtr_val_t gdtv);
