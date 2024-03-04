#include <malloc.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

#include "SimpleQueue.h"

struct SimpleQueueNode;
typedef struct SimpleQueueNode SimpleQueueNode;

struct SimpleQueueNode {
    _Atomic(SimpleQueueNode*) next;
    Value item;
};

SimpleQueueNode* SimpleQueueNode_new(Value item)
{
    SimpleQueueNode* node = (SimpleQueueNode*)malloc(sizeof(SimpleQueueNode));
    atomic_init(&node->next, NULL);
    node->item = item;
    return node;
}

struct SimpleQueue {
    SimpleQueueNode* head;
    SimpleQueueNode* tail;
    pthread_mutex_t head_mtx;
    pthread_mutex_t tail_mtx;
};


SimpleQueue* SimpleQueue_new(void)
{
    SimpleQueue* queue = (SimpleQueue*)malloc(sizeof(SimpleQueue));
    pthread_mutex_init(&queue->head_mtx, NULL);
    pthread_mutex_init(&queue->tail_mtx, NULL);
    queue->head = queue->tail = SimpleQueueNode_new(EMPTY_VALUE);

    return queue;
}

void SimpleQueue_delete(SimpleQueue* queue)
{
    SimpleQueueNode* current = queue->head;
    SimpleQueueNode* next;

    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }

    free(queue);
}

void SimpleQueue_push(SimpleQueue* queue, Value item)
{
    SimpleQueueNode* new_node = SimpleQueueNode_new(item);

    pthread_mutex_lock(&queue->tail_mtx);

    atomic_store(&queue->tail->next, new_node);
    queue->tail = new_node;

    pthread_mutex_unlock(&queue->tail_mtx);
}

Value SimpleQueue_pop(SimpleQueue* queue)
{
    Value ans = EMPTY_VALUE;

    pthread_mutex_lock(&queue->head_mtx);

    if (queue->head != queue->tail) {
        ans = atomic_load(&queue->head->next)->item;
        atomic_load(&queue->head->next)->item = EMPTY_VALUE;
        
        SimpleQueueNode* to_free = queue->head;
        queue->head = atomic_load(&queue->head->next);

        free(to_free);
    }

    pthread_mutex_unlock(&queue->head_mtx);

    return ans;
}

bool SimpleQueue_is_empty(SimpleQueue* queue)
{
    pthread_mutex_lock(&queue->head_mtx);

    bool ans = atomic_load(&queue->head->next) == NULL;

    pthread_mutex_unlock(&queue->head_mtx);

    return ans;
}
