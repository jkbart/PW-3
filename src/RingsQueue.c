#include <malloc.h>
#include <pthread.h>
#include <stdatomic.h>

#include <stdio.h>
#include <stdlib.h>

#include "HazardPointer.h"
#include "RingsQueue.h"

struct RingsQueueNode;
typedef struct RingsQueueNode RingsQueueNode;

struct RingsQueueNode {
    _Atomic(RingsQueueNode*) next;
    Value data[RING_SIZE];
    _Atomic(int) pop_idx;
    _Atomic(int) push_idx;
};

RingsQueueNode* RingsQueueNode_new() {
    RingsQueueNode* node = malloc(sizeof(RingsQueueNode));
    node->next           = NULL;
    atomic_store(&node->pop_idx, 0);
    atomic_store(&node->push_idx, 0);
    return node;
}

struct RingsQueue {
    RingsQueueNode* head;
    RingsQueueNode* tail;
    pthread_mutex_t pop_mtx;
    pthread_mutex_t push_mtx;
};

RingsQueue* RingsQueue_new(void)
{
    RingsQueue* queue = (RingsQueue*)malloc(sizeof(RingsQueue));

    queue->head       = RingsQueueNode_new();
    queue->tail       = queue->head;

    pthread_mutex_init(&queue->pop_mtx,  NULL);
    pthread_mutex_init(&queue->push_mtx, NULL);

    return queue;
}

void RingsQueue_delete(RingsQueue* queue)
{
    RingsQueueNode* current = queue->head;
    RingsQueueNode* next;

    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }
    
    free(queue);
}

void RingsQueue_push(RingsQueue* queue, Value item)
{
    pthread_mutex_lock(&queue->push_mtx);

    int push_id = atomic_load(&queue->tail->push_idx);

    if (push_id == RING_SIZE) {
        RingsQueueNode* new_node = RingsQueueNode_new();
        new_node->data[0] = item;
        atomic_store(&new_node->push_idx, 1);

        atomic_store(&queue->tail->next, new_node);
        queue->tail = queue->tail->next;
    } else {
       queue->tail->data[push_id] = item;
       atomic_store(&queue->tail->push_idx, push_id + 1);
    }

    pthread_mutex_unlock(&queue->push_mtx);
}

Value RingsQueue_pop(RingsQueue* queue)
{
    Value ans = EMPTY_VALUE;
    pthread_mutex_lock(&queue->pop_mtx);

    int pop_id  = atomic_load(&queue->head->pop_idx);
    int push_id = atomic_load(&queue->head->push_idx);
    RingsQueueNode* next_node = atomic_load(&queue->head->next);

    if (pop_id != push_id) {
        ans = queue->head->data[pop_id];
        atomic_store(&queue->head->pop_idx, pop_id + 1);
    } else if (next_node != NULL) {
        RingsQueueNode* old_node = queue->head;
        queue->head = next_node;
        free(old_node);

        int pop_id  = atomic_load(&queue->head->pop_idx);
        int push_id = atomic_load(&queue->head->push_idx);

        if (pop_id != push_id) {
            ans = queue->head->data[pop_id];
            atomic_store(&queue->head->pop_idx, pop_id + 1);
        }
    }

    pthread_mutex_unlock(&queue->pop_mtx);
    return ans;
}

bool RingsQueue_is_empty(RingsQueue* queue)
{
    pthread_mutex_lock(&queue->push_mtx);

    bool ans = (atomic_load(&queue->tail->pop_idx) == 
                atomic_load(&queue->tail->push_idx));

    pthread_mutex_unlock(&queue->push_mtx);

    return ans;
}
