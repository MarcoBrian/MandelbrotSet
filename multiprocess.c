
/************************************************************
* Filename: multiprocess.c
* Student name: Marco Brian Widjaja
* Student no.: 3035493024
* Date: Oct 31, 2019
* version: 1.1
* Development platform: Course VM
* Compilation: gcc multiprocess.c -o multiprocess -lSDL2 -lm
*************************************************************/


#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "Mandel.h"
#include "draw.h"
#define DEBUG 0


typedef struct message {
    int row_index;
    pid_t child_pid;
    float rowdata[IMAGE_WIDTH];
} MSG;

typedef struct task {
    int start_row;
    int num_of_rows;
} TASK;



int task_pipe[2];
int message_pipe[2];
int task_completed;
pid_t parent_pid;
float diff;

void sigint_handler(int signum);
void sigusr_handler(int signum);

int assign_task(int start_row , int workload){
    if(start_row+workload < 800){
      return workload;
    } else {
      return (800 - start_row );
    }
}


int main(int argc, char* args[]){

  //to measure parent process
  struct timespec start_time, end_time;
	clock_gettime(CLOCK_MONOTONIC, &start_time);

  //setting up

  parent_pid = getpid(); //storing parent_pid so that we can terminate all child process without terminating parent
  int number_of_child = atoi(args[1]); //set number of children from arguments
  int row_workload = atoi(args[2]); //set workload for each task
  signal(SIGINT,sigint_handler);   //install SIGINT handler
  signal(SIGUSR1,sigusr_handler);   //install SIGUSR1 handler
  pipe(task_pipe);   //create task_pipe
  pipe(message_pipe);   //create message_pipe
  int start_row = 0; // for assigning start_row of tasks

  task_completed = 0; //for storing the count of children's completed tasks

  //make multiple childs from same parent
  //parent process continue looping to create child process
  for(int i = 0 ; i < number_of_child ; i ++ ){
    pid_t pid = fork();
    if ( pid == 0 ){   //enter child process logic
      printf("Child(%d): Start up. Wait for task! \n", (int) getpid());
      while(1){
        pause();
      }
    }
}

sleep(1); //allow all childs to be set up first

for (int i = 0 ; i < number_of_child ; i++){
  TASK task ;
  task.start_row = start_row;
  task.num_of_rows = row_workload;
  write(task_pipe[1],&task, sizeof(TASK));
  kill(0, SIGUSR1);
  start_row += row_workload;
}


float * pixels;
//allocate memory to store the pixels
pixels = (float *) malloc(sizeof(float) * IMAGE_WIDTH * IMAGE_HEIGHT);
if (pixels == NULL) {
  printf("Out of memory!!\n");
  exit(1);
}



//to count how many have been returned, loop will end after reading 800 rows
int row_of_results = 0;
printf("Start collecting the image lines \n");
while(row_of_results!=800){
  //gather data
  MSG buffer;
  read(message_pipe[0],&buffer, sizeof(MSG));
  row_of_results++; //increment rows that is read

  // store data into pixels array
  for(int counter = 0 ; counter < IMAGE_WIDTH; counter++){
    pixels[buffer.row_index*IMAGE_WIDTH + counter] = buffer.rowdata[counter];
  }

  //assign new task to child process if it is idle
  if( buffer.child_pid > 0){
      if (start_row < 800){
          TASK task;
          task.start_row = start_row;
          int numrows = assign_task(start_row,row_workload);
          task.num_of_rows = numrows;
          start_row += numrows;
          write(task_pipe[1], &task , sizeof(TASK));
          kill(buffer.child_pid,SIGUSR1);
      }
  }

}


kill(0,SIGINT); // send interrupt to all child processes

for(int i=0 ; i < number_of_child; i++){
  int status;
  pid_t pid = wait(&status);
  printf("Child process ( %d ) terminated and completed %d tasks \n ",pid,WEXITSTATUS(status));
}

printf("All child process have completed \n");
struct rusage usage;
//children usage
getrusage(RUSAGE_CHILDREN, &usage);
struct timeval children_usertime = usage.ru_utime;
struct timeval children_systemtime = usage.ru_stime;

//children time in user mode
diff = (children_usertime.tv_usec/1000.0 + children_usertime.tv_sec * 1000);
printf("Total time spent by all child processes in user mode = %.3f ms \n" , diff);
//children time in system mode
diff = (children_systemtime.tv_usec/1000.0 + children_systemtime.tv_sec * 1000);
printf("Total time spent by all child processes in system mode = %.3f ms \n", diff);



//parent usage
getrusage(RUSAGE_SELF, &usage);
struct timeval parent_usertime = usage.ru_utime;
struct timeval parent_systemtime = usage.ru_stime;

diff = (parent_usertime.tv_usec/1000.0 + parent_usertime.tv_sec * 1000);
printf("Total time spent by parent process in user mode = %.3f ms \n" , diff);
//children time in system mode
diff = (parent_systemtime.tv_usec/1000.0 + parent_systemtime.tv_sec * 1000);
printf("Total time spent by parent process in system mode = %.3f ms \n", diff);

clock_gettime(CLOCK_MONOTONIC, &end_time);
diff = (end_time.tv_nsec - start_time.tv_nsec)/1000000.0 + (end_time.tv_sec - start_time.tv_sec)*1000.0;
printf("Total elapse time measured by parent process = %.3f ms\n", diff);

printf("Draw the image \n");
DrawImage(pixels, IMAGE_WIDTH, IMAGE_HEIGHT, "Mandelbrot demo", 10000);

}






void sigint_handler(int signum){
  if ((int) getpid() != (int) parent_pid){
  printf("Process ( %d ) is interrupted by ^C. Bye Bye \n" , (int) getpid());
  exit(task_completed);
  }
}

void sigusr_handler(int signum){

    if((int) getpid() != (int) parent_pid){

    TASK task_to_read;
    read(task_pipe[0], &task_to_read , sizeof(TASK));

    //allocate memory for computation of pixels
    float * long_row;
    long_row = (float *) malloc(sizeof(float) * IMAGE_WIDTH * task_to_read.num_of_rows);
    if (long_row == NULL) {
      printf("Out of memory!!\n");
      exit(1);
    }


    struct timespec start_compute, end_compute;
    printf("Child( %d ): Start the computation ...\n", (int) getpid());
    clock_gettime(CLOCK_MONOTONIC, &start_compute);

    //perform commputation
    for(int y = 0; y<task_to_read.num_of_rows;y++){
      for (int x=0; x < IMAGE_WIDTH ; x++){
        long_row[y*IMAGE_WIDTH + x] = Mandelbrot(x, y + task_to_read.start_row);
      }
    }
    clock_gettime(CLOCK_MONOTONIC, &end_compute);
    diff = (end_compute.tv_nsec - start_compute.tv_nsec)/1000000.0 + (end_compute.tv_sec - start_compute.tv_sec)*1000.0;
    printf("Child( %d ) ... completed. Elapse time = %.3f ms \n", (int) getpid() , diff);


    //write to pipe
    for(int y = 0; y<task_to_read.num_of_rows;y++){
      MSG message;
      message.row_index = task_to_read.start_row + y ;
      float new_row[IMAGE_WIDTH];
      for (int x=0; x < IMAGE_WIDTH ; x++){
        new_row[x] = long_row[y*IMAGE_WIDTH + x];
      }
      memcpy(message.rowdata, new_row, sizeof(new_row));

      if (y == task_to_read.num_of_rows - 1){
        message.child_pid = getpid();
      } else{
        message.child_pid = 0 ;
      }

      write(message_pipe[1], &message, sizeof(MSG));

    }
    free(long_row); //free memory used
    task_completed++; //increment completed tasks
  }
}
