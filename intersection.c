#include "arrivals.h"
#include "intersection_time.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern sem_t car_sem[4][4];
extern Car_Arrival curr_car_arrivals[4][4];
extern Car_Arrival input_car_arrivals[];

pthread_mutex_t intersection_mutex = PTHREAD_MUTEX_INITIALIZER;

void* manage_light(void* arg) {
    int side = ((int*)arg)[0];
    int direction = ((int*)arg)[1];
    free(arg);

    while (1) {
        sem_wait(&car_sem[side][direction]);
        int current_time = get_time_passed();
        if (current_time >= END_TIME) break;

        pthread_mutex_lock(&intersection_mutex);
        Car_Arrival car = curr_car_arrivals[side][direction];

        printf("traffic light %d %d turns green at time %d for car %d\n", 
               side, direction, current_time, car.id);
        sleep(CROSS_TIME);
        printf("traffic light %d %d turns red at time %d\n",
               side, direction, get_time_passed());

        pthread_mutex_unlock(&intersection_mutex);
    }
    return NULL;
}

void* supply_cars(void* arg) {
    start_time();
    for (int i = 0; i < NUM_ARRIVALS; i++) {
        while (get_time_passed() < input_car_arrivals[i].time) usleep(10000);
        Side side = input_car_arrivals[i].side;
        Direction dir = input_car_arrivals[i].direction;
        curr_car_arrivals[side][dir] = input_car_arrivals[i];
        sem_post(&car_sem[side][dir]);
    }
    return NULL;
}

void run_intersection() {
    pthread_t lights[4][4];
    pthread_t supply_thread;

    for (int side = 0; side < 4; side++) {
        for (int dir = 0; dir < 4; dir++) {
            if (dir == 3 && side != 2) continue; // UTURN only on SOUTH (side=2)
            int* args = malloc(2 * sizeof(int));
            args[0] = side;
            args[1] = dir;
            pthread_create(&lights[side][dir], NULL, manage_light, args);
        }
    }

    pthread_create(&supply_thread, NULL, supply_cars, NULL);
    pthread_join(supply_thread, NULL);

    for (int side = 0; side < 4; side++) {
        for (int dir = 0; dir < 4; dir++) {
            if (dir == 3 && side != 2) continue;
            pthread_join(lights[side][dir], NULL);
        }
    }
}