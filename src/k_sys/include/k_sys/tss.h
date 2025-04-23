
#pragma once

#include <stdint.h>

/*
 * Looks like I'll need to use this task state segment in the end :,(
 */

typedef struct _task_state_segment_t task_state_segment_t;

struct _task_state_segment_t {
    uint16_t link;
    uint16_t zeros0;

    uint32_t esp0;
    uint16_t ss0;
    uint16_t zeros1;

    uint32_t esp1;
    uint16_t ss1;
    uint16_t zeros2;

    uint32_t esp2;
    uint16_t ss2;
    uint16_t zeros3;

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

    uint16_t es;
    uint16_t zeros4;

    uint16_t cs;
    uint16_t zeros5;

    uint16_t ss;
    uint16_t zeros6;

    uint16_t ds;
    uint16_t zeros7;

    uint16_t fs;
    uint16_t zeros8;

    uint16_t gs;
    uint16_t zeros9;

    uint16_t ldt;
    uint16_t zeros10;

    uint16_t t;

    uint16_t iobm;
} __attribute__ ((packed));

void init_tss(task_state_segment_t *tss);
void flush_tss(uint32_t tss_index);


