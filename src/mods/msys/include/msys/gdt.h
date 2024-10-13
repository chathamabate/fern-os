

#ifndef MSYS_GDT_H
#define MSYS_GDT_H

#include <stdint.h>

struct _gdtr_val_t {
    uint16_t size;  // Comes first in memory.
    uint32_t offset;
} __attribute__ ((packed));

typedef struct _gdtr_val_t gdtr_val_t;

// Stores contents of the gdtr into where dest points.
// NOTE: This will write 
void read_gdtr(gdtr_val_t *dest);

#endif
