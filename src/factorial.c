#include <stdio.h>
#include <stdlib.h>

#include "cacti.h"

#define MSG_COMPUTE_FACTORIAL (message_type_t)0x1
#define MSG_INTRODUCE_TO_FATHER (message_type_t)0x2


typedef struct {
    unsigned long long k, k_fact, n;
} state_t;

role_t base_role;
role_t first_actor_role;

// Function reacting to message_type MSG_HELLO
void hello(void** stateptr, size_t nbytes, void* data) {
    (void) stateptr;
    (void) nbytes;
    message_t message_to_father = {MSG_INTRODUCE_TO_FATHER, 0,
                                   (void*) actor_id_self()};
    send_message((actor_id_t) data, message_to_father);
}

// Function reacting to message_type MSG_HELLO for first actor
void hello_master(void** stateptr, size_t nbytes, void* data) {
    (void) stateptr;
    (void) nbytes;
    (void) data;
}

// Function reacting to message_type MSG_COMPUTE_FACTORIAL
void compute_factorial(void** stateptr, size_t nbytes, void* data) {
    (void) nbytes;
    state_t* my_state = malloc(sizeof(state_t));
    *stateptr = my_state;

    state_t* father_state = data;
    my_state->k = father_state->k + 1;
    my_state->k_fact = father_state->k_fact * my_state->k;
    my_state->n = father_state->n;
    free(father_state);

    if (my_state->k < my_state->n) {
        message_t message_spawn = {MSG_SPAWN, 0, &base_role};
        if (send_message(actor_id_self(), message_spawn) == -1) {
            free(my_state);
        }
    } else {
        printf("%llu\n", my_state->k_fact);
        free(my_state);
        message_t message_to_myself = {MSG_GODIE, 0, NULL};
        send_message(actor_id_self(), message_to_myself);
    }
}


// Function reacting to message_type MSG_INTRODUCE_TO_FATHER
void pass_factorial_data(void** stateptr, size_t nbytes, void* data) {
    (void) nbytes;
    state_t* my_state = *stateptr;
    message_t message_to_son = {MSG_COMPUTE_FACTORIAL, 0, my_state};
    if (send_message((actor_id_t) data, message_to_son) == -1) {
        free(my_state);
    }
    message_t message_to_myself = {MSG_GODIE, 0, NULL};
    send_message(actor_id_self(), message_to_myself);
}

int main() {
    base_role.nprompts = 3;
    base_role.prompts = (act_t[]) {&hello, &compute_factorial,
                                   &pass_factorial_data};

    first_actor_role.nprompts = 3;
    first_actor_role.prompts = (act_t[]) {&hello_master, &compute_factorial,
                                          &pass_factorial_data};

    state_t* fact_data = malloc(sizeof(state_t));
    fact_data->k = 0;
    fact_data->k_fact = 1;
    scanf("%llu", &fact_data->n);

    actor_id_t first_actor_id;
    actor_system_create(&first_actor_id, &first_actor_role);

    message_t new_message = {MSG_COMPUTE_FACTORIAL, 0, fact_data};
    if (send_message(first_actor_id, new_message) == -1) {
        free(fact_data);
    }

    actor_system_join(first_actor_id);
    return 0;
}
