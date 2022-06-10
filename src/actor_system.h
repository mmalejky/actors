#ifndef CACTI_ACTOR_SYSTEM_H
#define CACTI_ACTOR_SYSTEM_H

typedef struct {
    message_t* store;
    size_t size, capacity, front, rear;
} message_queue_t;

typedef struct {
    message_queue_t* mq;
    role_t* role;
    bool is_dead;
    bool is_processed;
    void* state;
} actor_t;

typedef struct {
    actor_id_t* actor_queue;
    size_t aq_size, aq_capacity, aq_front, aq_rear;

    actor_t** store;
    size_t st_size, st_capacity;

    pthread_mutex_t work_mutex;
    pthread_cond_t work_cond;

    unsigned int alive_actor_count;
    unsigned int processed_actor_count;
    size_t working_thread_count;
    bool interrupted;
} actor_system_t;

message_queue_t* mq_create(actor_system_t* as);
void mq_destroy(message_queue_t* mq);
bool mq_empty(message_queue_t* mq);
void mq_enqueue(actor_system_t* as, message_queue_t* mq, message_t message);
message_t mq_dequeue(actor_system_t* as, message_queue_t* mq);

actor_t* actor_create(actor_system_t* as, role_t* role);
actor_system_t* as_create();
void as_destroy(actor_system_t** as);
bool as_correct_actor_id(actor_system_t* as, actor_id_t actor_id);
actor_t* as_get_actor(actor_system_t* as, actor_id_t actor_id);
actor_id_t as_add_actor(actor_system_t* as, role_t* role);

bool as_empty(actor_system_t* as);
void as_enqueue(actor_system_t* as, actor_id_t actor_id);
actor_id_t as_dequeue(actor_system_t* as);

void resize(size_t* capacity, size_t max_size);
void assert(actor_system_t* as, bool cond, char* message);

#endif //CACTI_ACTOR_SYSTEM_H
