#include "arrivals.h"
#include "intersection_time.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// External declarations (provided by template)
extern sem_t car_sem[4][4];
extern Car_Arrival curr_car_arrivals[4][4];
extern Car_Arrival input_car_arrivals[];

// Mutex for intersection access
pthread_mutex_t intersection_mutex = PTHREAD_MUTEX_INITIALIZER;

void* manage_light(void* arg) {
    int side = ((int*)arg)[0];
    int direction = ((int*)arg)[1];
    free(arg); // Free allocated memory

    while (1) {
        // Wait for a car at this light
        sem_wait(&car_sem[side][direction]);
        
        int current_time = get_time_passed();
        
        // Check for termination
        if (current_time >= END_TIME) {
            break;
        }

        // Acquire intersection lock
        pthread_mutex_lock(&intersection_mutex);
        
        // Get car details
        Car_Arrival car = curr_car_arrivals[side][direction];
        
        // Green light output
        printf("traffic light %d %d turns green at time %d for car %d\n",
               side, direction, current_time, car.id);
        
        // Simulate crossing time
        sleep(CROSS_TIME);
        
        // Red light output
        printf("traffic light %d %d turns red at time %d\n",
               side, direction, get_time_passed());
        
        // Release intersection
        pthread_mutex_unlock(&intersection_mutex);
    }
    return NULL;
}

void* supply_cars(void* arg) {
    start_time(); // Initialize timer
    
    for (int i = 0; i < NUM_ARRIVALS; i++) {
        // Wait until scheduled arrival time
        while (get_time_passed() < input_car_arrivals[i].time) {
            usleep(10000); // 10ms sleep to avoid busy waiting
        }
        
        // Store arrival and signal semaphore
        Side side = input_car_arrivals[i].side;
        Direction dir = input_car_arrivals[i].direction;
        curr_car_arrivals[side][dir] = input_car_arrivals[i];
        sem_post(&car_sem[side][dir]);
    }
    return NULL;
}

void run_intersection() {
    pthread_t light_threads[4][4]; // [NORTH, EAST, SOUTH, WEST] x [LEFT, STRAIGHT, RIGHT, UTURN]
    pthread_t supply_thread;

    // Create traffic light threads
    for (int side = NORTH; side <= WEST; side++) {
        for (int dir = LEFT; dir <= UTURN; dir++) {
            // Handle U-turn restriction
            if (dir == UTURN && side != SOUTH) continue;
            
            int* args = malloc(2 * sizeof(int));
            args[0] = side;
            args[1] = dir;
            pthread_create(&light_threads[side][dir], NULL, manage_light, args);
        }
    }

    // Create and wait for supply thread
    pthread_create(&supply_thread, NULL, supply_cars, NULL);
    pthread_join(supply_thread, NULL);

    // Cleanup all light threads
    for (int side = NORTH; side <= WEST; side++) {
        for (int dir = LEFT; dir <= UTURN; dir++) {
            if (dir == UTURN && side != SOUTH) continue;
            pthread_join(light_threads[side][dir], NULL);
        }
    }
}