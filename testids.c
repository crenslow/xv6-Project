//Program to test getting and setting UID, GID, and getting PPID


#ifdef CS333_P2
#include "types.h"
#include "user.h"


int
main(void)
{
   uint uid, gid, ppid;

   uid = getuid();
   printf(2, "Current UID is: %d\n", uid);
   printf(2, "Setting UID to 100 \n");
   if(setuid(100) < 0)
     printf(2,"Something went wrong\n");
   uid = getuid();
   printf(2, "Current UID is %d\n", uid);

   gid = getgid();
   printf(2, "Current GID is: %d\n", gid);
   printf(2, "Setting GID to 200 \n");
   if(setgid(200) < 0)
     printf(2,"Something went wrong\n");
   gid = getgid();
   printf(2, "Current UID is %d\n\n", gid);
	
   printf(2,"Trying to set UID to invalid number(-1)");
   printf(2, "Setting UID to -1 \n");
   if(setuid(-1) < 0)
     printf(2,"This is an invalid UID\n");
  
   printf(2,"Trying to set UID to invalid number(32768)");
   printf(2, "Setting UID to 32768 \n");
   if(setuid(32768) < 0)
     printf(2,"This is an invalid UID\n");

   printf(2,"Trying to set GID to invalid number(-1)");
   printf(2, "Setting GID to -1 \n");
   if(setgid(-1) < 0)
     printf(2,"This is an invalid GID\n");
  
   printf(2,"Trying to set GID to invalid number(32768)");
   printf(2, "Setting GID to 32768 \n");
   if(setgid(32768) < 0)
     printf(2,"This is an invalid UID\n");

   ppid = getppid();
   printf(2, "My parent process is: %d\n", ppid);
   printf(2, "Done!\n");

   exit();
}

#endif //CS333_P2
