
#include "s_data/fixed_queue.h"
#include "s_util/str.h"

fixed_queue_t *new_fixed_queue(allocator_t *al, size_t cs, size_t cap) {
    if (!al || cs == 0 || cap == 0) {
        return NULL;
    }

    fixed_queue_t *fq = al_malloc(al, sizeof(fixed_queue_t));
    void *buffer = al_malloc(al, cs * cap);

    if (!fq || !buffer) {
        al_free(al, fq);
        al_free(al, buffer);

        return NULL;
    }

    // Success.

    *(allocator_t **)&(fq->al) = al;
    *(size_t *)&(fq->cell_size) = cs;
    *(size_t *)&(fq->capacity) = cap;
    *(void **)&(fq->buffer) = buffer;

    fq->start = 0;
    fq->end = 0;
    fq->empty = true;

    return fq;
}

void delete_fixed_queue(fixed_queue_t *fq) {
    if (!fq) {
        return;
    }

    al_free(fq->al, fq->buffer);
    al_free(fq->al, fq);
}

bool fq_enqueue(fixed_queue_t *fq, const void *ele, bool writeover) {
    if (!ele) {
        return false;
    }

    if (fq->empty) {
        mem_cpy(fq->buffer + (fq->cell_size * fq->end), ele, fq->cell_size);
        fq->empty = false;

        return true;
    }

    // Ok, non-empty!

    const size_t new_end = (fq->end + 1) % fq->capacity;

    if (new_end == fq->start) { // The buffer is full!
        if (!writeover) {
            return false;
        }

        // Write over is requested!

        mem_cpy(fq->buffer + (fq->cell_size * new_end), ele, fq->cell_size);
        
        // Advance end AND start.
        fq->end = new_end;
        fq->start = (fq->start + 1) % fq->capacity;

        return true;
    }

    // If the buffer was not full, we can just write to new end and call it a day.
    mem_cpy(fq->buffer + (fq->cell_size * new_end), ele, fq->cell_size);
    fq->end = new_end;

    return true;
}

bool fq_poll(fixed_queue_t *fq, void *dest) {
    if (!dest || fq->empty) {
        return false;
    }

    mem_cpy(dest, fq->buffer + (fq->start * fq->cell_size), fq->cell_size);

    if (fq->start == fq->end) {
        fq->empty = true;
    } else {
        fq->start = (fq->start + 1) % fq->capacity;
    }

    return true;
}
