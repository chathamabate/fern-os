
#include "s_data/term_buffer.h"

term_buffer_t *new_term_buffer(allocator_t *al, term_cell_t default_cell, uint16_t rows, uint16_t cols) {
    term_buffer_t *tb = al_malloc(al, sizeof(term_buffer_t));

    term_cell_t *buf = NULL;
    if (rows > 0 && cols > 0) {
        buf = al_malloc(al, sizeof(term_cell_t) * rows * cols);
        if (!buf) {
            al_free(al, tb);
            return NULL;
        }
    }

    // Success!
    *(allocator_t **)&(tb->al) = al;
    *(term_cell_t *)&(tb->default_cell) = default_cell;
    tb->rows = rows;
    tb->cols = cols;
    tb->cursor_row = 0;
    tb->cursor_col = 0;
    tb->buf = buf;

    return tb; 
}

void delete_term_buffer(term_buffer_t *tb) {
    if (tb && tb->al) {
        al_free(tb->al, tb->buf); 
        al_free(tb->al, tb);
    }
}

void tb_clear(term_buffer_t *tb, term_cell_t cell_val) {
    for (size_t r = 0; r < tb->rows; r++) {
        for (size_t c = 0; c < tb->cols; c++) {
            tb->buf[(tb->cols * r) + c] = cell_val;
        }
    }

    tb->cursor_row = 0;
    tb->cursor_col = 0;
}

fernos_error_t tb_resize(term_buffer_t *tb, uint16_t rows, uint16_t cols) {
    if (!(tb->al)) {
        return FOS_E_STATE_MISMATCH;
    }

    size_t new_size = rows * cols * sizeof(term_cell_t);
    term_cell_t *new_buf = al_realloc(tb->al, tb->buf, new_size);

    if (new_size > 0 && !new_buf) {
        return FOS_E_NO_MEM;
    }

    // Success!

    tb->rows = rows;
    tb->cols = cols;
    tb->buf = new_buf;

    tb_clear(tb, tb->default_cell);

    return FOS_E_SUCCESS;
}

