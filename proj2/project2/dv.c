/*****************************************************************************
* Distance vector routing protocol without reverse path poisoning.           *
*****************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "routing-simulator.h"

// Message format to send between nodes
typedef struct {
    cost_t distance[MAX_NODES];
    node_t next_hop[MAX_NODES];
} data_t;

// State format
typedef struct {
    data_t distance_vector[MAX_NODES];
    int flag; // Flag to indicate whether the state has changed
} state_t;

// Bellman-Ford algorithm to update the entire state
void run_bellman_ford(void *state) {
    state_t *node_state = (state_t *)state;

    for (node_t destination = get_first_node(); destination <= get_last_node(); destination = get_next_node(destination)) {
        cost_t min_cost = COST_INFINITY;
        node_t best_hop = -1;

        for (node_t neighbor = get_first_node(); neighbor <= get_last_node(); neighbor = get_next_node(neighbor)) {
            if (get_link_cost(neighbor) != COST_INFINITY && !(get_current_node() == neighbor && destination != neighbor)) {
                cost_t total_cost = COST_ADD(node_state->distance_vector[neighbor].distance[destination], get_link_cost(neighbor));

                // Check for infinity to avoid overflow
                if (total_cost < min_cost) {
                    min_cost = total_cost;
                    best_hop = neighbor;
                }
            }
        }

        // Update the state if there is a change in cost or next_hop
        if ((min_cost != node_state->distance_vector[get_current_node()].distance[destination] || best_hop != node_state->distance_vector[get_current_node()].next_hop[destination]) && min_cost != 0) {
            node_state->flag = 1; // Set the flag to indicate a change
            node_state->distance_vector[get_current_node()].distance[destination] = min_cost;
            node_state->distance_vector[get_current_node()].next_hop[destination] = best_hop;
            set_route(destination, best_hop, min_cost);
        }
    }
}

// Handler for the node to allocate and initialize its state
void *init_state() {
    state_t *state = (state_t *)calloc(1, sizeof(state_t));

    // Initialize distance and next_hop arrays
    for (node_t node1 = get_first_node(); node1 <= get_last_node(); node1 = get_next_node(node1)) {
        for (node_t neighbour = get_first_node(); neighbour <= get_last_node(); neighbour = get_next_node(neighbour)) {
            state->distance_vector[node1].distance[neighbour] = (node1 == neighbour) ? 0 : COST_INFINITY;
            state->distance_vector[node1].next_hop[neighbour] = (node1 == neighbour) ? neighbour : -1;
        }
    }

    state->flag = 0; // Initialize the flag to indicate no change
    return state;
}

// Notify a node that a neighboring link has changed cost
void notify_link_change(void *state, node_t neighbor, cost_t new_cost) {
    state_t *node_state = (state_t *)state;

    // Backup current state
    state_t backup_state = *node_state;

    // Update the distance and next_hop arrays based on the new cost
    node_state->distance_vector[get_current_node()].distance[neighbor] = new_cost;
    node_state->distance_vector[get_current_node()].next_hop[neighbor] = neighbor; // Direct link
    set_route(neighbor, neighbor, new_cost);

    // Run Bellman-Ford to update the entire state
    run_bellman_ford(node_state);

    // If the state has changed, send messages to neighbors
    if (memcmp(&backup_state, node_state, sizeof(state_t)) != 0) {
        for (node_t destination = get_first_node(); destination <= get_last_node(); destination = get_next_node(destination)) {
            if (destination != get_current_node() && get_link_cost(destination) != COST_INFINITY) {
                // Prepare the message with updated data
                data_t update_data = node_state->distance_vector[get_current_node()];

                message_t update_message;
                update_message.data = &update_data;
                update_message.size = sizeof(data_t);

                // Send the message to the neighboring node
                send_message(destination, update_message);
            }
        }
    }
}

// Receive a message sent by a neighboring node
void notify_receive_message(void *state, node_t sender, message_t message) {
    state_t *node_state = (state_t *)state;
    data_t *received_data = (data_t *)message.data;

    // Backup current state
    state_t backup_state = *node_state;

    // Update the distance and next_hop arrays based on the received message
    for (node_t destination = get_first_node(); destination <= get_last_node(); destination = get_next_node(destination)) {
        node_state->distance_vector[sender].distance[destination] = received_data->distance[destination];
    }
    node_state->flag = 0; // Reset the flag

    // Run Bellman-Ford to update the entire state
    run_bellman_ford(node_state);

    // If the state has changed, send messages to neighbors
    if(node_state->flag==1){
        node_state->flag=0;
        if (memcmp(&backup_state, node_state, sizeof(state_t)) != 0) {
            for (node_t destination = get_first_node(); destination <= get_last_node(); destination = get_next_node(destination)) {
                if (destination != get_current_node() && get_link_cost(destination) != COST_INFINITY) {
                    // Prepare the message with updated data
                    data_t update_data = node_state->distance_vector[get_current_node()];

                    message_t update_message;
                    update_message.data = &update_data;
                    update_message.size = sizeof(data_t);

                    // Send the message to the neighboring node
                    send_message(destination, update_message);
                }
            }
        }
    }
}
