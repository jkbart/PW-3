#include <malloc.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "BLQueue.h"
#include "HazardPointer.h"

struct BLNode;
typedef struct BLNode BLNode;
typedef _Atomic(BLNode*) AtomicBLNodePtr;

struct BLNode {
    AtomicBLNodePtr next;
    _Atomic(Value) buffer[BUFFER_SIZE];
    _Atomic(int) pop_idx;
    _Atomic(int) push_idx;
};

BLNode* BLNode_new() {
    BLNode* node = malloc(sizeof(BLNode));
    atomic_store(&node->next, NULL);
    for (int i = 0; i < BUFFER_SIZE; i++)
        atomic_store(&node->buffer[i], EMPTY_VALUE);
    atomic_store(&node->pop_idx, 0);
    atomic_store(&node->push_idx, 0);
    return node;
}

struct BLQueue {
    AtomicBLNodePtr head;
    AtomicBLNodePtr tail;
    HazardPointer hp;
};

BLQueue* BLQueue_new(void)
{
    BLQueue* queue = (BLQueue*)malloc(sizeof(BLQueue));
    BLNode* node = BLNode_new();
    atomic_store(&queue->head, node);
    atomic_store(&queue->tail, node);
    HazardPointer_initialize(&queue->hp);
    return queue;
}

void BLQueue_delete(BLQueue* queue)
{
    BLNode* current = atomic_load(&queue->head);
    BLNode* node_to_free;

    while (current != NULL) {
        node_to_free = current;
        current = atomic_load(&current->next);
        free(node_to_free);
    }

    HazardPointer_finalize(&queue->hp);

    free(queue);
}

void BLQueue_push(BLQueue* queue, Value item)
{
    while (true) {
        BLNode* tail = HazardPointer_protect(&queue->hp, &queue->tail);     // 1
        int push_idx = atomic_fetch_add(&tail->push_idx, 1);                // 2

        if (push_idx < BUFFER_SIZE) { // 3a
            if (!atomic_compare_exchange_strong(&tail->buffer[push_idx], &EMPTY_VALUE, item)) { // *
                continue;
            } else { // *
                return;
            }
        } else { // 3b
            BLNode* next = atomic_load(&tail->next);
            if (next != NULL) { // 4a
                atomic_compare_exchange_strong(&queue->tail, &tail, next);
                continue;
            } else { // 4b
                BLNode* new_tail = BLNode_new();
                atomic_store(&new_tail->buffer[0], item);
                atomic_store(&new_tail->push_idx, 1);
                if (!atomic_compare_exchange_strong(&tail->next, &next, new_tail)) { // *
                    continue;
                } else { // *
                    atomic_store(&queue->tail, new_tail);
                    return;
                }
            }
        }
    }
}

Value BLQueue_pop(BLQueue* queue)
{
    while (true) {
        BLNode* head = HazardPointer_protect(&queue->hp, &queue->head);     // 1
        int pop_idx = atomic_fetch_add(&head->pop_idx, 1);                  // 2

        if (pop_idx < BUFFER_SIZE) { // 3a
            Value ans = atomic_exchange(&head->buffer[pop_idx], TAKEN_VALUE);
            if (ans == EMPTY_VALUE) { // *
                continue;
            } else { // *
                return ans;
            }
        } else { // 3b
            BLNode* next = atomic_load(&head->next);
            if (next == NULL) { // 4a
                return EMPTY_VALUE;
            } else { // 4b
                if (atomic_compare_exchange_strong(&queue->head, &head, next)) {
                    HazardPointer_retire(&queue->hp, head);
                }
                continue;
            }
        }
    }
}

bool BLQueue_is_empty(BLQueue* queue)
{
    while (true) {
        BLNode* tail = HazardPointer_protect(&queue->hp, &queue->tail);

        int pop_idx_1 = atomic_load(&tail->pop_idx); 
        int push_idx = atomic_load(&tail->push_idx); 
        int pop_idx_2 = atomic_load(&tail->pop_idx);

        if (atomic_load(&queue->tail) != tail) {
            if (pop_idx_2 < push_idx && push_idx < BUFFER_SIZE)
                return false;
            continue;
        }

        if ((pop_idx_1 < push_idx) == (pop_idx_2 < push_idx)) {
            return pop_idx_1 >= push_idx || pop_idx_2 >= BUFFER_SIZE;
        }
    }
}
