/*
 * Operating Systems (2INC0) Practical Assignment
 * Threading
 *
 * Intersection Part 2
 * 
 * Joao Guilherme Cesse Valenca Calado de Freitas (1942395)
 * Dylan Galiart (1942115)
 * Jazman Ismail (1923072)
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <stdbool.h>
 #include <unistd.h>
 #include <errno.h>
 #include <pthread.h>
 #include <semaphore.h>
 
 #include "arrivals.h"
 #include "intersection_time.h"
 #include "input.h"
 
 // TODO: Global variables: mutexes, data structures, etc...
 
 /* 
  * curr_car_arrivals[][][]
  *
  * A 3D array that stores the arrivals that have occurred
  * The first two indices determine the entry lane: first index is Side, second index is Direction
  * curr_arrivals[s][d] returns an array of all arrivals for the entry lane on side s for direction d,
  *   ordered in the same order as they arrived
  */
 static Car_Arrival curr_car_arrivals[4][4][20];
 
 /*
  * car_sem[][]
  *
  * A 2D array that defines a semaphore for each entry lane,
  *   which are used to signal the corresponding traffic light that a car has arrived
  * The two indices determine the entry lane: first index is Side, second index is Direction
  */
 static sem_t car_sem[4][4];
 
 // Holds the mutexes for the intersection
 typedef struct {
   int side;
   int direction;
   pthread_mutex_t** mtx_squares;
   pthread_mutex_t** mtx_exit_lanes;
   int current_index;  // Track the index of the next car to process
 } Light_Args;
 
 /*
  * supply_cars()
  *
  * A function for supplying car arrivals to the intersection
  * This should be executed by a separate thread
  */
 static void* supply_cars()
 {
   int t = 0;
   int num_curr_arrivals[4][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};
 
   // for every arrival in the list
   for (int i = 0; i < sizeof(input_car_arrivals)/sizeof(Car_Arrival); i++)
   {
     // get the next arrival in the list
     Car_Arrival arrival = input_car_arrivals[i];
      sleep_until_arrival(arrival.time); // Use the correct timing function
      t = arrival.time;
     // store the new arrival in curr_arrivals
     curr_car_arrivals[arrival.side][arrival.direction][num_curr_arrivals[arrival.side][arrival.direction]] = arrival;
     num_curr_arrivals[arrival.side][arrival.direction] += 1;
     // increment the semaphore for the traffic light that the arrival is for
     sem_post(&car_sem[arrival.side][arrival.direction]);
   }
 
   return NULL;
 }
 
 
 /*
  * manage_light(void* arg)
  *
  * A function that implements the behaviour of a traffic light
  */
 static void* manage_light(void* arg) {
  Light_Args* args = (Light_Args*)arg;
  int side = args->side;
  int direction = args->direction;
  int current_index = args->current_index;
  pthread_mutex_t** mtx_squares = args->mtx_squares;
  pthread_mutex_t** mtx_exit_lanes = args->mtx_exit_lanes;

  int exit_lane = (side + direction + 1) % 4;

  // Determine which squares to lock based on direction and side
  pthread_mutex_t* square1;
  pthread_mutex_t* square2;

  if (direction == LEFT || direction == STRAIGHT) {
      // Example mapping for LEFT/STRAIGHT (adjust based on intersection layout)
      square1 = mtx_squares[(side + 1) % 4]; // Next square
      square2 = mtx_squares[(side + 2) % 4]; // Opposite square
  } else if (direction == RIGHT) {
      square1 = mtx_squares[side]; // Current square
      square2 = mtx_squares[(side + 3) % 4]; // Previous square
  } else { // UTURN
      square1 = mtx_squares[side]; // Current square
      square2 = mtx_squares[(side + 2) % 4]; // Opposite square
  }

  while (1) {
      struct timespec timeout;
      clock_gettime(CLOCK_REALTIME, &timeout);
      int time_remaining = END_TIME - get_time_passed();
      if (time_remaining <= 0) break;
      timeout.tv_sec += time_remaining;

      if (sem_timedwait(&car_sem[side][direction], &timeout) == -1) {
          if (errno == ETIMEDOUT) break;
          perror("sem_timedwait");
          break;
      }

      int current_time = get_time_passed();
      if (current_time >= END_TIME) break;

      // Lock only specific squares and exit lane (NO GLOBAL MUTEX)
      pthread_mutex_lock(mtx_exit_lanes[exit_lane]);
      pthread_mutex_lock(square1);
      pthread_mutex_lock(square2);

      // Process car
      Car_Arrival car = curr_car_arrivals[side][direction][current_index];
      current_index++;
      args->current_index = current_index;

      printf("traffic light %d %d turns green at time %d for car %d\n", 
              side, direction, current_time, car.id);
      sleep(CROSS_TIME);
      printf("traffic light %d %d turns red at time %d\n",
              side, direction, get_time_passed());

      // Unlock mutexes
      pthread_mutex_unlock(square2);
      pthread_mutex_unlock(square1);
      pthread_mutex_unlock(mtx_exit_lanes[exit_lane]);
  }

  free(args);
  return NULL;
}
 
 
 int main(int argc, char * argv[])
 {
  pthread_mutex_t mtx_squares[4]; // One mutex per square
  pthread_mutex_t mtx_exit_lanes[4]; // One mutex per exit lane
  pthread_mutex_t* mtxs_squares[4];
  pthread_mutex_t* mtxs_exit_lanes[4];
 
   // Initialize mutexes for exit lanes
   for (int i = 0; i < 4; i++) {
    pthread_mutex_init(&mtx_squares[i], NULL);
    pthread_mutex_init(&mtx_exit_lanes[i], NULL);
    mtxs_squares[i] = &mtx_squares[i];
    mtxs_exit_lanes[i] = &mtx_exit_lanes[i];
}
 
   // create semaphores to wait/signal for arrivals
   for (int i = 0; i < 4; i++)
   {
     for (int j = 0; j < 4; j++)
     {
       sem_init(&car_sem[i][j], 0, 0);
     }
   }

   // start the timer
   start_time();
   
   // TODO: create a thread per traffic light that executes manage_light
   pthread_t lights[4][4]; // Traffic light threads (4 sides × 4 directions)
   pthread_t supply_thread; // Car arrival thread
 
   // Create traffic light threads
   for (int side = 0; side < 4; side++) {          // NORTH=0, EAST=1, SOUTH=2, WEST=3
       for (int dir = 0; dir < 4; dir++) {         // LEFT=0, STRAIGHT=1, RIGHT=2, UTURN=3
           // Skip U-turn lanes for non-south sides
           if (dir == 3 && side != 2) continue; 
 
           // Pass side/direction as arguments
           Light_Args* light_args = malloc(sizeof(Light_Args));
           light_args->side = side;
           light_args->direction = dir;
           light_args->mtx_squares = mtxs_squares;
           light_args->mtx_exit_lanes = mtxs_exit_lanes;
           light_args->current_index = 0;  // Initialize index
           pthread_create(&lights[side][dir], NULL, manage_light, light_args);
       }
   }
 
   // TODO: create a thread that executes supply_cars()
   pthread_create(&supply_thread, NULL, supply_cars, NULL);
   pthread_join(supply_thread, NULL); // Wait for car arrivals to finish
 
   // Cleanup: Wait for all traffic light threads
   for (int side = 0; side < 4; side++) {
       for (int dir = 0; dir < 4; dir++) {
           if (dir == 3 && side != 2) continue; // Skip invalid U-turn threads
           pthread_join(lights[side][dir], NULL);
       }
   }
   // TODO: wait for all threads to finish
 
   // This was not done in Jaz's code so idk what to do here
 
   // destroy mutexes
   for (int i = 0; i < 4; i++)
   {
     pthread_mutex_destroy(&mtx_exit_lanes[i]);
     pthread_mutex_destroy(&mtx_squares[i]);
   }
   // destroy mutexes
   for (int i = 0; i < 4; i++)
   {
     free(mtxs_exit_lanes[i]);
     free(mtxs_squares[i]);
   }
 
   // destroy semaphores
   for (int i = 0; i < 4; i++)
   {
     for (int j = 0; j < 4; j++)
     {
       sem_destroy(&car_sem[i][j]);
     }
   }
 }
 