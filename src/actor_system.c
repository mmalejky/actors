#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>

#include "cacti.h"
#include "actor_system.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define BASE_MQ_CAPACITY 1
#define BASE_AQ_CAPACITY 1
#define BASE_AS_CAPACITY 1


message_queue_t* mq_create(actor_system_t* as) {
    message_queue_t* mq = calloc(1, sizeof(message_queue_t));
    assert(as, mq != NULL, "No memory for new message queue");
    mq->capacity = MIN(BASE_MQ_CAPACITY, ACTOR_QUEUE_LIMIT);
    mq->store = calloc(mq->capacity, sizeof(message_t));
    assert(as, mq->store != NULL, "No memory for new message queue");
    return mq;
}

void mq_destroy(message_queue_t* mq) {
    free(mq->store);
    free(mq);
}

bool mq_empty(message_queue_t* mq) {
    return mq->size == 0;
}

void mq_enqueue(actor_system_t* as, message_queue_t* mq, message_t message) {
    assert(as, mq->size < ACTOR_QUEUE_LIMIT,
           "Maximum number of messages on message queue exceeded");
    if (mq->size == mq->capacity) {
        resize(&mq->capacity, ACTOR_QUEUE_LIMIT);
        mq->store = realloc(mq->store, mq->capacity * sizeof(message_t));
        assert(as, mq->store != NULL,
               "No memory for next message on message queue");
        if (mq->size != 0) {
            memmove(mq->store + mq->front + mq->capacity - mq->size,
                    mq->store + mq->front,
                    (mq->size - mq->front) * sizeof(message_t));
            mq->front += mq->capacity - mq->size;
        }

    }
    mq->store[mq->rear] = message;
    ++mq->size;
    mq->rear = (mq->rear + 1) % mq->capacity;
}

message_t mq_dequeue(actor_system_t* as, message_queue_t* mq) {
    assert(as, !mq_empty(mq), "Dequeue on empty message queue");
    --mq->size;
    size_t old_front = mq->front;
    mq->front = (mq->front + 1) % mq->capacity;
    return mq->store[old_front];
}


actor_t* actor_create(actor_system_t* as, role_t* const role) {
    actor_t* actor = calloc(1, sizeof(actor_t));
    assert(as, actor != NULL, "No memory for new actor");
    actor->role = role;
    actor->mq = mq_create(as);
    return actor;
}

actor_system_t* as_create() {
    actor_system_t* as = calloc(1, sizeof(actor_system_t));
    if (as == NULL) {
        return NULL;
    }
    //assert(as, as != NULL, "No memory for actor system");
    as->aq_capacity = MIN(BASE_AQ_CAPACITY, CAST_LIMIT);
    as->actor_queue = calloc(as->aq_capacity, sizeof(actor_id_t));
    if (as->actor_queue == NULL) {
        free(as);
        return NULL;
    }
    //assert(as, as->actor_queue != NULL, "No memory for actor system");
    as->st_capacity = MIN(BASE_AS_CAPACITY, CAST_LIMIT);
    as->store = calloc(as->st_capacity, sizeof(actor_t*));
    if (as->store == NULL) {
        free(as->actor_queue);
        free(as);
        return NULL;
    }
    //assert(as, as->store != NULL, "No memory for actor system");
    if (pthread_mutex_init(&(as->work_mutex), NULL) != 0) {
        free(as->actor_queue);
        free(as->store);
        free(as);
        return NULL;
    }
    if (pthread_cond_init(&(as->work_cond), NULL) != 0) {
        free(as->actor_queue);
        free(as->store);
        free(as);
        return NULL;
    }
    as->alive_actor_count = 0;
    as->processed_actor_count = 0;
    as->working_thread_count = POOL_SIZE;
    return as;
}

void as_destroy(actor_system_t** as) {
    if (*as == NULL) {
        return;
    }
    for (size_t i = 0; i < (*as)->st_size; ++i) {
        mq_destroy((*as)->store[i]->mq);
        free((*as)->store[i]);
    }
    free((*as)->actor_queue);
    free((*as)->store);
    pthread_mutex_destroy(&((*as)->work_mutex));
    pthread_cond_destroy(&((*as)->work_cond));
    free(*as);
    *as = NULL;
}

bool as_correct_actor_id(actor_system_t* as, actor_id_t actor_id) {
    return actor_id >= 0 && (unsigned long) actor_id < as->st_size;
}

actor_t* as_get_actor(actor_system_t* as, actor_id_t actor_id) {
    if (!as_correct_actor_id(as, actor_id)) {
        return NULL;
    }
    return as->store[actor_id];
}

actor_id_t as_add_actor(actor_system_t* as, role_t* const role) {
    assert(as, as->st_size < CAST_LIMIT,
           "Maximum number of actors exceeded");
    actor_t* actor = actor_create(as, role);
    if (as->st_size == as->st_capacity) {
        resize(&as->st_capacity, CAST_LIMIT);
        as->store = realloc(as->store, as->st_capacity * sizeof(actor_t*));
        assert(as, as->store != NULL,
               "No memory for next actor on actor list");
    }
    as->store[as->st_size] = actor;
    ++as->alive_actor_count;
    return as->st_size++;
}


bool as_empty(actor_system_t* as) {
    return as->aq_size == 0;
}

void as_enqueue(actor_system_t* as, actor_id_t actor_id) {
    assert(as, as->aq_size < CAST_LIMIT,
           "Maximum number of actors on actors queue exceeded");
    if (as->aq_size == as->aq_capacity) {
        resize(&as->aq_capacity, CAST_LIMIT);
        as->actor_queue = realloc(as->actor_queue,
                                  as->aq_capacity * sizeof(actor_id_t));
        assert(as, as->actor_queue != NULL,
               "No memory for next actor on actor queue");
        if (as->aq_size != 0) {
            memmove(as->actor_queue + as->aq_front + as->aq_capacity -
                    as->aq_size, as->actor_queue + as->aq_front,
                    (as->aq_size - as->aq_front) * sizeof(actor_id_t));
            as->aq_front += as->aq_capacity - as->aq_size;
        }
    }
    as->actor_queue[as->aq_rear] = actor_id;
    ++as->aq_size;
    as->aq_rear = (as->aq_rear + 1) % as->aq_capacity;
}

actor_id_t as_dequeue(actor_system_t* as) {
    assert(as, !as_empty(as), "Dequeue on empty actor queue");
    --as->aq_size;
    size_t old_front = as->aq_front;
    as->aq_front = (as->aq_front + 1) % as->aq_capacity;
    return as->actor_queue[old_front];
}


void resize(size_t* capacity, size_t max_size) {
    if (*capacity == 0) {
        *capacity = 1;
        return;
    }
    if (*capacity * 2 > max_size) {
        *capacity = max_size;
        return;
    }
    *capacity = *capacity * 2;
}

void assert(actor_system_t* as, bool cond, char* message) {
    if (!cond) {
        as_destroy(&as);
        printf("%s.\n", message);
        exit(1);
    }
}
