#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "cacti.h"

#define num_t int
#define MSG_COMPUTE_CELL (message_type_t)0x1
#define MSG_INTRODUCE_TO_FATHER (message_type_t)0x2
#define MSG_PASS_CREATION_DATA (message_type_t)0x3
#define MSG_TO_MASTER (message_type_t)0x4


typedef struct {
    num_t** number;
    num_t** time;
    num_t matrix_width;
    num_t matrix_height;
    num_t calculated_row_count;
    num_t column_number;
    actor_id_t son_id;
    actor_id_t master_id;
} state_t;

typedef struct {
    state_t* base_state;
    num_t master_message_count;
    num_t* sum;
} master_state_t;

typedef struct {
    num_t row_number;
    num_t current_sum;
} calculation_data_t;

role_t base_role;
role_t master_role;

// Function reacting to message_type MSG_HELLO
void hello(void** stateptr, size_t nbytes, void* data) {
    (void) stateptr;
    (void) nbytes;
    message_t message_to_father = {MSG_INTRODUCE_TO_FATHER, 0,
                                   (void*) actor_id_self()};
    send_message((actor_id_t) data, message_to_father);
}

// Function reacting to message_type MSG_HELLO for master
void hello_master(void** stateptr, size_t nbytes, void* data) {
    (void) stateptr;
    (void) nbytes;
    (void) data;
}

// Function reacting to message_type MSG_COMPUTE_CELL
void compute_cell(void** stateptr, size_t nbytes, void* data) {
    (void) nbytes;
    if (*stateptr == NULL) {
        free(data);
        return;
    }
    state_t* my_state = *stateptr;
    calculation_data_t* calc_data = data;
    calculation_data_t* n_calc_data = calloc(1, sizeof(calculation_data_t));
    n_calc_data->row_number = calc_data->row_number;
    usleep(my_state->time[calc_data->row_number][my_state->column_number] *
           1000);
    n_calc_data->current_sum = calc_data->current_sum +
                               my_state->number[calc_data->row_number][my_state->column_number];
    free(calc_data);
    ++my_state->calculated_row_count;
    if (my_state->column_number == my_state->matrix_width - 1) {
        message_t message_to_master = {MSG_TO_MASTER, 0, n_calc_data};
        if (send_message(my_state->master_id, message_to_master) == -1) {
            free(n_calc_data);
            free(my_state);
            *stateptr = NULL;
            return;
        }
    } else {
        message_t message_to_son = {MSG_COMPUTE_CELL, 0, n_calc_data};
        if (send_message(my_state->son_id, message_to_son) == -1) {
            free(n_calc_data);
            free(my_state);
            *stateptr = NULL;
            return;
        }
    }
    if (my_state->calculated_row_count == my_state->matrix_height) {
        free(my_state);
        message_t message_to_myself = {MSG_GODIE, 0, NULL};
        send_message(actor_id_self(), message_to_myself);
    }
}

// Function reacting to message_type MSG_TO_MASTER
void react_master(void** stateptr, size_t nbytes, void* data) {
    (void) nbytes;
    master_state_t* my_state = *stateptr;
    ++my_state->master_message_count;
    if (my_state->master_message_count == my_state->base_state->matrix_width) {
        for (num_t i = 0; i < my_state->base_state->matrix_height; ++i) {
            calculation_data_t* calc_data = calloc(1,
                                                   sizeof(calculation_data_t));
            usleep(my_state->base_state->time[i][my_state->base_state->column_number] *
                   1000);
            calc_data->current_sum = my_state->base_state->number[i][my_state->base_state->column_number];
            calc_data->row_number = i;
            message_t message_to_son = {MSG_COMPUTE_CELL, 0, calc_data};
            if (send_message(my_state->base_state->son_id, message_to_son) ==
                -1) {
                free(calc_data);
                free(my_state->base_state);
                free(my_state->sum);
                free(my_state);
                return;
            }
        }
    } else if (my_state->master_message_count >
               my_state->base_state->matrix_width) {
        calculation_data_t* calc_data = data;
        my_state->sum[calc_data->row_number] = calc_data->current_sum;
        free(calc_data);
        if (my_state->master_message_count ==
            my_state->base_state->matrix_width +
            my_state->base_state->matrix_height) {
            for (num_t i = 0; i < my_state->base_state->matrix_height; ++i) {
                printf("%d\n", my_state->sum[i]);
            }
            free(my_state->base_state);
            free(my_state->sum);
            free(my_state);
            message_t message_to_myself = {MSG_GODIE, 0, NULL};
            send_message(actor_id_self(), message_to_myself);
        }
    }
}

// Function reacting to message_type MSG_INTRODUCE_TO_FATHER
void pass_creation_data(void** stateptr, size_t nbytes, void* data) {
    (void) nbytes;
    state_t* my_state = *stateptr;
    my_state->son_id = (actor_id_t) data;
    message_t message_to_son = {MSG_PASS_CREATION_DATA, 0, my_state};
    if (send_message((actor_id_t) data, message_to_son) == -1) {
        free(my_state);
        return;
    }
    message_t message_to_master = {MSG_TO_MASTER, 0, NULL};
    send_message(my_state->master_id, message_to_master);
}

// Function reacting to message_type MSG_INTRODUCE_TO_FATHER for master
void pass_creation_data_master(void** stateptr, size_t nbytes, void* data) {
    (void) nbytes;
    master_state_t* my_state = *stateptr;
    my_state->base_state->son_id = (actor_id_t) data;
    message_t message_to_son = {MSG_PASS_CREATION_DATA, 0,
                                my_state->base_state};
    if (send_message((actor_id_t) data, message_to_son) == -1) {
        free(my_state->base_state);
        free(my_state->sum);
        free(my_state);
        return;
    }
    message_t message_to_yourself = {MSG_TO_MASTER, 0, NULL};
    if (send_message(my_state->base_state->master_id, message_to_yourself) ==
        -1) {
        free(my_state->sum);
        free(my_state);
    }
}

// Function reacting to message_type MSG_PASS_CREATION_DATA
void create_yourself(void** stateptr, size_t nbytes, void* data) {
    (void) nbytes;
    state_t* my_state = calloc(1, sizeof(state_t));
    *stateptr = my_state;
    state_t* father_state = data;
    my_state->number = father_state->number;
    my_state->time = father_state->time;
    my_state->matrix_width = father_state->matrix_width;
    my_state->matrix_height = father_state->matrix_height;
    my_state->column_number = father_state->column_number + 1;
    my_state->master_id = father_state->master_id;
    if (my_state->column_number < my_state->matrix_width - 1) {
        message_t message_spawn = {MSG_SPAWN, 0, &base_role};
        if (send_message(actor_id_self(), message_spawn) == -1) {
            free(my_state);
        }
    } else {
        message_t message_to_master = {MSG_TO_MASTER, 0, NULL};
        if (send_message(my_state->master_id, message_to_master) == -1) {
            free(my_state);
        }
    }
}

// Function reacting to message_type MSG_PASS_CREATION_DATA for master
void create_yourself_master(void** stateptr, size_t nbytes, void* data) {
    (void) nbytes;
    master_state_t* my_state = data;
    *stateptr = my_state;
    message_t message_spawn = {MSG_SPAWN, 0, &base_role};
    if (send_message(actor_id_self(), message_spawn) == -1) {
        free(my_state->base_state);
        free(my_state->sum);
        free(my_state);
    }
}


void input(num_t*** number, num_t*** time, num_t* k, num_t* n) {
    scanf("%d", k);
    scanf("%d", n);
    *number = calloc(*k, sizeof(num_t*));
    *time = calloc(*k, sizeof(num_t*));
    for (num_t i = 0; i < *k; ++i) {
        (*number)[i] = calloc(*n, sizeof(num_t));
        (*time)[i] = calloc(*n, sizeof(num_t));
    }
    for (num_t i = 0; i < *k; ++i) {
        for (num_t j = 0; j < *n; ++j) {
            scanf("%d", &((*number)[i][j]));
            scanf("%d", &((*time)[i][j]));
        }
    }
}

void clear(num_t** number, num_t** time, num_t k) {
    for (num_t i = 0; i < k; ++i) {
        free(number[i]);
        free(time[i]);
    }
    free(number);
    free(time);
}

int main() {
    num_t** number;
    num_t** time;
    num_t k, n;
    input(&number, &time, &k, &n);

    base_role.nprompts = 4;
    base_role.prompts = (act_t[]) {&hello, &compute_cell, &pass_creation_data,
                                   &create_yourself};

    master_role.nprompts = 5;
    master_role.prompts = (act_t[]) {&hello_master, NULL,
                                     &pass_creation_data_master,
                                     &create_yourself_master, &react_master};

    actor_id_t master_id;
    actor_system_create(&master_id, &master_role);

    master_state_t* master_creation_data = calloc(1, sizeof(state_t));
    master_creation_data->base_state = calloc(1, sizeof(state_t));
    master_creation_data->sum = calloc(k, sizeof(num_t));
    master_creation_data->master_message_count = 0;
    master_creation_data->base_state->matrix_height = k;
    master_creation_data->base_state->matrix_width = n;
    master_creation_data->base_state->calculated_row_count = 0;
    master_creation_data->base_state->number = number;
    master_creation_data->base_state->time = time;
    master_creation_data->base_state->column_number = 0;
    master_creation_data->base_state->master_id = master_id;

    message_t new_message = {MSG_PASS_CREATION_DATA, 0, master_creation_data};
    if (send_message(master_id, new_message) == -1) {
        free(master_creation_data->base_state);
        free(master_creation_data->sum);
        free(master_creation_data);
    }

    actor_system_join(master_id);
    clear(number, time, k);
    return 0;
}

