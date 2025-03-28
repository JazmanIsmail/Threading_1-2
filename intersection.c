#include "arrivals.h"
#include "intersection_time.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern sem_t car_sem[4][4];           // Semaphores for each traffic light
extern Car_Arrival curr_car_arrivals[4][4]; // Stores active car arrivals
extern Car_Arrival input_car_arrivals[];    // Predefined car arrival data

pthread_mutex_t intersection_mutex = PTHREAD_MUTEX_INITIALIZER; // Intersection lock

void* manage_light(void* arg) {
    // Extract side and direction from arguments
    int side = ((int*)arg)[0];
    int direction = ((int*)arg)[1];
    free(arg); // Free allocated memory for arguments

    while (1) {
        // Wait for a car to arrive at this light
        sem_wait(&car_sem[side][direction]);

        // Check if simulation time has ended
        int current_time = get_time_passed();
        if (current_time >= END_TIME) break;

        // Lock the intersection for exclusive access
        pthread_mutex_lock(&intersection_mutex);

        // Retrieve car details
        Car_Arrival car = curr_car_arrivals[side][direction];

        // Green light phase
        printf("traffic light %d %d turns green at time %d for car %d\n", 
               side, direction, current_time, car.id);
        sleep(CROSS_TIME); // Simulate crossing time

        // Red light phase
        printf("traffic light %d %d turns red at time %d\n",
               side, direction, get_time_passed());

        // Release the intersection
        pthread_mutex_unlock(&intersection_mutex);
    }
    return NULL;
}

void* supply_cars(void* arg) {
    start_time(); // Initialize simulation timer

    for (int i = 0; i < NUM_ARRIVALS; i++) {
        // Wait until the car's scheduled arrival time
        while (get_time_passed() < input_car_arrivals[i].time) {
            usleep(10000); // Non-busy waiting (10ms delay)
        }

        // Notify the corresponding traffic light
        Side side = input_car_arrivals[i].side;
        Direction dir = input_car_arrivals[i].direction;
        curr_car_arrivals[side][dir] = input_car_arrivals[i];
        sem_post(&car_sem[side][dir]); // Signal semaphore
    }
    return NULL;
}
void run_intersection() {
    pthread_t lights[4][4]; // Traffic light threads (4 sides × 4 directions)
    pthread_t supply_thread; // Car arrival thread

    // Create traffic light threads
    for (int side = 0; side < 4; side++) {          // NORTH=0, EAST=1, SOUTH=2, WEST=3
        for (int dir = 0; dir < 4; dir++) {         // LEFT=0, STRAIGHT=1, RIGHT=2, UTURN=3
            // Skip U-turn lanes for non-south sides
            if (dir == 3 && side != 2) continue; 

            // Pass side/direction as arguments
            int* args = malloc(2 * sizeof(int));
            args[0] = side;
            args[1] = dir;
            pthread_create(&lights[side][dir], NULL, manage_light, args);
        }
    }

    // Start car arrival thread
    pthread_create(&supply_thread, NULL, supply_cars, NULL);
    pthread_join(supply_thread, NULL); // Wait for car arrivals to finish

    // Cleanup: Wait for all traffic light threads
    for (int side = 0; side < 4; side++) {
        for (int dir = 0; dir < 4; dir++) {
            if (dir == 3 && side != 2) continue; // Skip invalid U-turn threads
            pthread_join(lights[side][dir], NULL);
        }
    }
}