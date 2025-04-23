
#pragma once

#include <stdint.h>

/*
 * Looks like I'll need to use this task state segment in the end :,(
 */

typedef struct _task_state_segment_t task_state_segment_t;

struct _task_state_segment_t {
    uint32_t link;

    uint32_t esp0;
    uint32_t ss0;

    uint32_t esp1;
    uint32_t ss1;

    uint32_t esp2;
    uint32_t ss2;

    uint32_t cr3;

    uint32_t eip;

    uint32_t eflags;

    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;

    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;

    uint32_t ldt;

    uint16_t t;

    uint16_t iobm;
} __attribute__ ((packed));

void init_tss(task_state_segment_t *tss);
void flush_tss(uint32_t tss_index);


