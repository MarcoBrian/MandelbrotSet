
/************************************************************
* Filename: multithread.c
* Student name: Marco Brian Widjaja
* Student no.: 3035493024
* Date: Oct 24, 2019
* version: 1.1
* Development platform: Course VM
* Compilation: gcc multithread.c -o multithread -lSDL2 -lm -pthread
*************************************************************/


#include <semaphore.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "Mandel.h"
#include "draw.h"



//define TASK struct
typedef struct task {
    int start_row;
    int num_of_rows;
    int terminate; 
} TASK;


TASK assign_task(int start_row , int workload){
    TASK new_task;
    new_task.start_row = start_row;
    new_task.terminate = 0;
    if(start_row+workload < 800){
      new_task.num_of_rows = workload;
      return new_task;
      
    } else {
      new_task.num_of_rows = 800 - start_row;
      return new_task;
    }
}

TASK create_finishing_task(void){
    TASK temp = {0,0,1};
    return temp; 
}

float * pixels; 
TASK *task_buffer; //create a task buffer
pthread_t * worker_threads;


//to keep track of which memory of taskbuffer to fill or use
//in the producer and consumer thread
int fill = 0;
int use = 0;
int buffer_size;

//semaphore for empty and full (cond variable)
sem_t empty;
sem_t full;
sem_t mutex; //semaphore for shared resource (mutex)




//put function for the producer
void put(TASK task){
  task_buffer[fill] = task;
  fill = (fill + 1) % buffer_size;
}

//get function for consumer
TASK get(void){
  TASK temp = task_buffer[use];
  use = (use + 1) % buffer_size;
  return temp;
}

//TODO change  
void *consumer(void *arg){
  TASK temp;
  int * count;
  count = (int*) malloc(sizeof(int));
  int worker_number = *((int *) arg);
  printf("Worker(%d):Start up wait for tasks!\n", worker_number);
  while(1){
    sem_wait(&full);
    sem_wait(&mutex);
    temp = get();
    sem_post(&mutex);
    sem_post(&empty);
    if (temp.terminate == 1){
      break;
    }else{
        //read and compute the image
        
        //allocate memory for computation of pixels
        float * long_row;
        long_row = (float *) malloc(sizeof(float) * IMAGE_WIDTH * temp.num_of_rows);
        if (long_row == NULL) {
        printf("Out of memory!!\n");
        exit(1);
        }

        
        struct timespec start_compute, end_compute;
        printf("Worker(%d): Start the computation ...\n", worker_number);
        clock_gettime(CLOCK_MONOTONIC, &start_compute);
        
        //perform computation
        for(int y = 0; y<temp.num_of_rows;y++){
        for (int x=0; x < IMAGE_WIDTH ; x++){
            long_row[y*IMAGE_WIDTH + x] = Mandelbrot(x, y + temp.start_row);
        }
        }

        
        float diff;

        clock_gettime(CLOCK_MONOTONIC, &end_compute);
        diff = (end_compute.tv_nsec - start_compute.tv_nsec)/1000000.0 + (end_compute.tv_sec - start_compute.tv_sec)*1000.0;
        printf("Worker(%d) ... completed. Elapse time = %.3f ms \n", worker_number , diff);

        //store into pixels
        //starting point = temp.start_row*IMAGE_WIDTH + x 
        
        int position = temp.start_row * IMAGE_WIDTH ;
        for(int y = 0; y<temp.num_of_rows;y++){
        for (int x=0; x < IMAGE_WIDTH ; x++){
            pixels[position++] = long_row[y*IMAGE_WIDTH + x];
        }
        }

        *count = *count + 1;

    }
  }
  

  //cleaning up, returning value to main thread
  pthread_exit((void *) count);


}




int main(int argc , char * argv[]){
    
  //to measure parent process
  struct timespec start_time, end_time;
  clock_gettime(CLOCK_MONOTONIC, &start_time);

  int number_of_workers = atoi(argv[1]);
  int number_of_rows_per_task = atoi(argv[2]);
  buffer_size = atoi(argv[3]);
  task_buffer = (TASK *) malloc(sizeof(TASK)* buffer_size);

  //allocate memory to store the pixels
  pixels = (float *) malloc(sizeof(float) * IMAGE_WIDTH * IMAGE_HEIGHT);
  if (pixels == NULL) {
  printf("Out of memory!!\n");
  exit(1);
  }


  //initilize sempahores
  if(sem_init(&full,0,0)==-1){
    printf("ERROR initializing FULL sempahore \n");
    exit(1);
  }

  if(sem_init(&empty,0,buffer_size)==-1){
    printf("ERROR initializing EMPTY sempahore \n");
    exit(1);
  }

  if(sem_init(&mutex,0,1)==-1){
    printf("ERROR initializing MUTEX sempahore \n");
    exit(1);
  }

  //create threads
  worker_threads = (pthread_t *) malloc(sizeof(pthread_t) * number_of_workers); //create threads array
  for(int i = 0; i < number_of_workers ; i++){
    int *arg = (int*) malloc(sizeof(int));
    *arg = i;
    pthread_create(&worker_threads[i],NULL,consumer, arg);
  }

  //distibute all tasks
  int start_row = 0 ;
  while(start_row < 800){
    TASK new_task = assign_task(start_row,number_of_rows_per_task);
    start_row += number_of_rows_per_task;
    sem_wait(&empty);
    sem_wait(&mutex);
    put(new_task);
    sem_post(&mutex);
    sem_post(&full);
  }

  //once task distributed send termination message in the task_buffer, amount as much as workers

  for(int i = 0; i < number_of_workers; i++){
    TASK temp = create_finishing_task();
    sem_wait(&empty);
    sem_wait(&mutex);
    put(temp);
    sem_post(&mutex);
    sem_post(&full);
  }


  //Node * head = (Node*) malloc(sizeof(Node));

  int * return_values = (int *) malloc(sizeof(int)*number_of_workers);


  int * temp_values;
  for(int i = 0; i < number_of_workers; i++){
    pthread_join(worker_threads[i], (void**) &temp_values);
    return_values[i] = *temp_values;
  }

  for(int i =0 ; i< number_of_workers;i++){
    printf("thread %d completed %d tasks \n", i, return_values[i]); 
  }

  

  printf("All worker threads have terminated \n");





  //destroy threads


  //free memory
  free(return_values);
  free(task_buffer);

  //destroy semaphores after used
  if(sem_destroy(&full)==-1){
    printf("ERROR destroying FULL sempahore \n");
    exit(1);
  }
  if(sem_destroy(&empty)==-1){
    printf("ERROR destroying EMPTY sempahore \n");
    exit(1);
  }
  if(sem_destroy(&mutex)==-1){
    printf("ERROR destroying MUTEX sempahore \n");
    exit(1);
  }



//process usage
struct rusage usage;
getrusage(RUSAGE_SELF, &usage);
struct timeval parent_usertime = usage.ru_utime;
struct timeval parent_systemtime = usage.ru_stime;
float diff;

diff = (parent_usertime.tv_usec/1000.0 + parent_usertime.tv_sec * 1000);
printf("Total time spent by process in user mode = %.3f ms \n" , diff);
//children time in system mode
diff = (parent_systemtime.tv_usec/1000.0 + parent_systemtime.tv_sec * 1000);
printf("Total time spent by process in system mode = %.3f ms \n", diff);

clock_gettime(CLOCK_MONOTONIC, &end_time);
diff = (end_time.tv_nsec - start_time.tv_nsec)/1000000.0 + (end_time.tv_sec - start_time.tv_sec)*1000.0;
printf("Total elapse time measured by process = %.3f ms\n", diff);


printf("Draw the image \n");
DrawImage(pixels, IMAGE_WIDTH, IMAGE_HEIGHT, "Mandelbrot demo", 10000);
free(pixels);

}
