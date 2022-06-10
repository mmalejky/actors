#include <pthread.h>
#include <stdbool.h>
#include <signal.h>

#include "cacti.h"
#include "actor_system.h"


pthread_t threads[POOL_SIZE];
static pthread_key_t current_actor_id;
actor_system_t* as = NULL;
struct sigaction action;
sigset_t old_mask;

void catch(int signo) {
    if (signo == SIGINT) {
        as->interrupted = true;
        for (size_t i = 0; i < as->st_size; ++i) {
            as->store[i]->is_dead = true;
        }
        as->alive_actor_count = 0;
        pthread_cond_broadcast(&(as->work_cond));
    }
}

void* worker() {
    sigset_t sigint_mask;
    sigemptyset(&sigint_mask);
    sigaddset(&sigint_mask, SIGINT);
    pthread_sigmask(SIG_BLOCK, &sigint_mask, NULL);
    assert(as, sigaction(SIGINT, &action, NULL) != -1,
           "Sigaction error");
    while (true) {
        pthread_mutex_lock(&(as->work_mutex));
        pthread_sigmask(SIG_UNBLOCK, &sigint_mask, NULL);
        pthread_sigmask(SIG_BLOCK, &sigint_mask, NULL);

        while (as_empty(as) && !(as->alive_actor_count <= 0 &&
                                 as->processed_actor_count <= 0)) {
            pthread_cond_wait(&(as->work_cond), &(as->work_mutex));
        }
        if (as_empty(as)) {
            break;
        }
        actor_id_t id = as_dequeue(as);
        actor_t* actor = as_get_actor(as, id);
        pthread_setspecific(current_actor_id, (void*) id);
        actor->is_processed = true;
        as->processed_actor_count++;
        message_t message = mq_dequeue(as, actor->mq);

        if (message.message_type == MSG_GODIE) {
            if (!actor->is_dead) {
                actor->is_dead = true;
                --as->alive_actor_count;
            }
        } else if (message.message_type == MSG_SPAWN) {
            if (!as->interrupted) {
                role_t* role = message.data;
                actor_id_t new_actor_id = as_add_actor(as, role);
                message_t hello_message = {MSG_HELLO, 0, (void*) id};
                pthread_mutex_unlock(&(as->work_mutex));
                send_message(new_actor_id, hello_message);
                pthread_mutex_lock(&(as->work_mutex));
            }
        } else {
            pthread_mutex_unlock(&(as->work_mutex));
            actor->role->prompts[message.message_type](&actor->state,
                                                       message.nbytes,
                                                       message.data);
            pthread_mutex_lock(&(as->work_mutex));
        }
        actor->is_processed = false;
        as->processed_actor_count--;
        if (!mq_empty(actor->mq)) {
            as_enqueue(as, id);
            pthread_cond_broadcast(&(as->work_cond));
        }
        pthread_mutex_unlock(&(as->work_mutex));
    }
    --as->working_thread_count;
    bool end = as->working_thread_count == 0;
    pthread_cond_broadcast(&(as->work_cond));
    pthread_mutex_unlock(&(as->work_mutex));
    if (end) {
        as_destroy(&as);
    }
    return NULL;
}

int actor_system_create(actor_id_t* actor, role_t* const role) {
    as = as_create();
    if (as == NULL) {
        return -1;
    }
    pthread_mutex_lock(&(as->work_mutex));
    actor_id_t first_actor_id = as_add_actor(as, role);
    pthread_mutex_unlock(&(as->work_mutex));
    *actor = first_actor_id;

    sigset_t block_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGINT);
    pthread_sigmask(SIG_BLOCK, &block_mask, &old_mask);

    pthread_key_create(&current_actor_id, NULL);
    sigset_t catcher_block_mask;
    sigemptyset(&catcher_block_mask);
    action.sa_handler = catch;
    action.sa_mask = catcher_block_mask;
    action.sa_flags = SA_RESTART;
    for (int i = 0; i < POOL_SIZE; ++i) {
        pthread_t thread;
        pthread_create(&thread, NULL, worker, NULL);
        threads[i] = thread;
    }

    message_t hello_message = {MSG_HELLO, 0, NULL};
    send_message(first_actor_id, hello_message);
    return 0;
}

int send_message(actor_id_t actor, message_t message) {
    pthread_mutex_lock(&(as->work_mutex));
    actor_t* found_actor = as_get_actor(as, actor);
    if (found_actor == NULL) {
        pthread_mutex_unlock(&(as->work_mutex));
        return -2;
    }
    if (found_actor->is_dead) {
        pthread_mutex_unlock(&(as->work_mutex));
        return -1;
    }
    if (mq_empty(found_actor->mq) && !found_actor->is_processed) {
        as_enqueue(as, actor);
    }
    mq_enqueue(as, found_actor->mq, message);
    pthread_cond_broadcast(&(as->work_cond));
    pthread_mutex_unlock(&(as->work_mutex));
    return 0;
}

void actor_system_join(actor_id_t actor) {
    assert(as, as != NULL, "No active actor system");
    assert(as, as_correct_actor_id(as, actor), "Invalid actor_id");
    for (int i = 0; i < POOL_SIZE; ++i) {
        pthread_join(threads[i], NULL);
    }
    pthread_sigmask(SIG_SETMASK, &old_mask, NULL);
}

actor_id_t actor_id_self() {
    return (actor_id_t) pthread_getspecific(current_actor_id);
}
