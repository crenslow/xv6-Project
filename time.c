//time.c


#ifdef CS333_P2

#include "types.h"
#include "user.h"

int
main(int argc, char* argv[])
{
 // exec("echo", argv);
  int timebefore = uptime(); 
  int timeafter = 0;
  int pid = fork();
	  
  if(pid > 0){
     pid = wait();
     timeafter = uptime();
  }
  else if(pid == 0){
     if(argv[1] != NULL){
       if(exec(argv[1], argv +1) < 0)
	 exit();
     }  
     else
       exit();	     
  }   
  else
     printf(1,"fork error\n");  
  int timeelapsed = (timeafter - timebefore) / 1000;
  int timedec = (timeafter - timebefore) % 1000;
  printf(1,"%s ran in %d.", argv[1] , timeelapsed);
  if(timedec < 100 && timedec > 9)
    printf(1, "0%d", timedec);
  if(timedec < 10)
    printf(1, "00%d", timedec);
  if(timedec >= 100)
    printf(1,"%d", timedec);

  printf(1, " seconds.\n");







  exit();
}  



  




































#endif //CS333_P2
