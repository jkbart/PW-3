#include <malloc.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "HazardPointer.h"
#include "LLQueue.h"

struct LLNode;
typedef struct LLNode LLNode;
typedef _Atomic(LLNode*) AtomicLLNodePtr;

struct LLNode {
    AtomicLLNodePtr next;
    _Atomic Value item;
};

LLNode* LLNode_new(Value item)
{
    LLNode* node = (LLNode*)malloc(sizeof(LLNode));
    node->next = NULL;
    atomic_store(&node->item, item); 
    return node;
}

struct LLQueue {
    AtomicLLNodePtr head;
    AtomicLLNodePtr tail;
    HazardPointer hp;
};

LLQueue* LLQueue_new(void)
{
    LLQueue* queue = (LLQueue*)malloc(sizeof(LLQueue));
    AtomicLLNodePtr node = LLNode_new(EMPTY_VALUE);

    HazardPointer_initialize(&queue->hp);

    atomic_store(&queue->head, node);
    atomic_store(&queue->tail, node);

    return queue;
}

void LLQueue_delete(LLQueue* queue)
{
    LLNode* current = atomic_load(&queue->head);
    LLNode* node_to_free;

    // No need to free() in _delete using HP.
    while (current != NULL) {
        node_to_free = current;
        current = atomic_load(&current->next);
        free(node_to_free);
    }

    HazardPointer_finalize(&queue->hp);

    free(queue);
}

void LLQueue_push(LLQueue* queue, Value item)
{
    AtomicLLNodePtr new_node;
    atomic_store(&new_node, LLNode_new(item));
    while (true) {
        LLNode* tail = HazardPointer_protect(&queue->hp, &queue->tail); // 1
        LLNode* next = atomic_load(&tail->next);

        if (next == NULL && atomic_compare_exchange_strong(&tail->next, &next, new_node)) { // 2
            atomic_compare_exchange_strong(&queue->tail, &tail, new_node); // 3a
            HazardPointer_clear(&queue->hp);
            return;
        } else { // 3b
            atomic_compare_exchange_strong(&queue->tail, &tail, next);
            continue;
        }
    }
}

Value LLQueue_pop(LLQueue* queue)
{
    while (true) {
        LLNode* head = HazardPointer_protect(&queue->hp, &queue->head); // 1
        Value first_item = atomic_exchange(&head->item, EMPTY_VALUE); // 2
        LLNode* next = atomic_load(&head->next);

        if (first_item != EMPTY_VALUE) { // 3a
            if (next != NULL) { // queue update
                if (atomic_compare_exchange_strong(&queue->head, &head, next)) {
                    HazardPointer_retire(&queue->hp, head);
                }
            }
            HazardPointer_clear(&queue->hp);
            return first_item;
        } else { // 3b
            if (next == NULL){
                HazardPointer_clear(&queue->hp);
                return EMPTY_VALUE;
            } else { // queue update
                if (atomic_compare_exchange_strong(&queue->head, &head, next)) {
                    HazardPointer_retire(&queue->hp, head);
                }
            }
        }
    }
}

bool LLQueue_is_empty(LLQueue* queue)
{
    LLNode* head = HazardPointer_protect(&queue->hp, &queue->head);

    bool ans = atomic_load(&head->item) == EMPTY_VALUE && 
        atomic_load(&head->next) == NULL;

    HazardPointer_clear(&queue->hp);

    return ans;
}
