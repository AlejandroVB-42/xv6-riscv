#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char names[3] = {'A', 'B', 'C'};
  int reps = 20;                    // por defecto 20 repeticiones

  if(argc > 1)
    reps = atoi(argv[1]);           // opcional: rr <repeticiones>

  for(int i = 0; i < 3; i++){
    int pid = fork();
    if(pid < 0){
      printf("fork failed\n");
      exit(1);
    }
    if(pid == 0){
      for(int r = 0; r < reps; r++){
        for(volatile int k = 0; k < 10000000; k++)
          ;                         // trabajo que consume CPU
        write(1, &names[i], 1);     // imprime una letra
      }
      exit(0);
    }
  }
  for(int i = 0; i < 3; i++)
    wait(0);
  write(1, "\n", 1);
  exit(0);
}
