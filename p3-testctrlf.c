//to test ctrl-f and list
//

#include "types.h"
#include "user.h"


int 
main(){
  int pid;


    pid = fork();
  if(pid < 0) {
    printf(1, "failure");
  }
  if(pid == 0) {
    printf(1, "A child process has been created.");
    sleep(3 * TPS);
    exit();
  }else {
     sleep(3 * TPS);
  }
  if(pid != 0) {
    wait();
    printf(1, "Child exited");
    }


  exit();
}
