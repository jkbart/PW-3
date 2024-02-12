#include <malloc.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>

#include "HazardPointer.h"

thread_local int _thread_id = -1;
int _num_threads = -1;

void HazardPointer_register(int thread_id, int num_threads)
{
    _thread_id = thread_id;
    _num_threads = num_threads;
}

void HazardPointer_initialize(HazardPointer* hp)
{
    for (int i = 0; i < MAX_THREADS; i++) {
        hp->pointer[i] = NULL;
        for (int j = 0; j < MAX_THREADS; j++)
            hp->retired[i][j] = NULL;
    }
}

void HazardPointer_finalize(HazardPointer* hp)
{
    for (int i = 0; i < MAX_THREADS; i++) {
        hp->pointer[i] = NULL;
        for (int j = 0; j < MAX_THREADS; j++) {
            free(hp->retired[i][j]);
            hp->retired[i][j] = NULL;
        }
    }
}

void* HazardPointer_protect(HazardPointer* hp, const _Atomic(void*)* atom)
{
    // Assuming HP_clear was called.
    while (true) {
        atomic_store(&hp->pointer[thread_id], atomic_load(atom));
        if (atomic_load(atom) == hp->pointer[thread_id])
            break;
    }
    return hp->pointer[thread_id];
}

void HazardPointer_clear(HazardPointer* hp)
{
    atomic_store(&hp->pointer[thread_id], NULL);
}

void HazardPointer_retire(HazardPointer* hp, void* ptr)
{
    // Check if there is empty spot:
    for (int i = 0; i < RETIRED_THRESHOLD; i++) {
        if (hp->retired[thread_id][i] == NULL) {
            hp->retired[thread_id][i] = ptr;
            return;
        }
    }

    while (true) { // In case RETIRED_THRESHOLD is smaller than num_threads.
        for (int i = 0; i < RETIRED_THRESHOLD; i++) {
            bool used = false;

            for (int j = 0; j < num_threads; j++) {
                if (atomic_load(&hp->pointer[j]) == hp->retired[thread_id][i]) {
                    used = true;
                    break;
                }
            }

            if (used == false) {
                free(hp->retired[thread_id][i]);
                hp->retired[thread_id][i] = ptr;
                return;
            }
        }
    }
}
