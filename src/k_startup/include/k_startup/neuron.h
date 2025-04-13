
#pragma once

#include "s_util/err.h"
#include "k_startup/fwd_defs.h"

/*
 * Abstractly, a neuron is a way of signalling that an event has happened.
 *
 */

typedef struct _neuron_t neuron_t;
typedef struct _neuron_inode_t neuron_inode_t;
typedef struct _neuron_leaf_t neuron_leaf_t;

struct _neuron_t {

};

void delete_neuron(neuron_t *nr);

void nr_fire(neuron_t *nr);

fernos_error_t nr_bind_input(neuron_t *nr, neuron_inode_t *nri);

void nr_unbind_input(neuron_t *nr, neuron_inode_t *nri);

struct _neuron_inode_t {
    // Has internal neuron inputs, and internal neuron outputs.
};

fernos_error_t nri_bind_output(neuron_inode_t *nri, neuron_t *nr);
void nri_unbind_output(neuron_inode_t *nri, neuron_t *nr);

struct _neuron_leaf_t {
    // Points to one waiting thread.
};

// Firing the neuron leaf is going to take some actual work here!!

fernos_error_t nrl_set_listener(neuron_leaf_t *nrl, thread_id_t tid);
