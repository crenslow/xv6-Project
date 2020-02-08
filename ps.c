//ps.c


#ifdef CS333_P2
#include "types.h"
#include "user.h"
#include "uproc.h"

int
main(void)
{

  int max = 72;
  struct uproc * table =  malloc(max *(sizeof(struct uproc)));
  if(getprocs(max, table) < 0)
    printf(1, "Error: Processes cannot be gotten.\n");
#ifdef CS333_P4  
  printf(2,"\nPID\tName         UID\tGID\tPPID\tPrio\tElapsed\tCPU\tState\tSize\n");
#else
  printf(2,"\nPID\tName         UID\tGID\tPPID\tElapsed\tCPU\tState\tSize\n");
#endif
   for(int i = 0; i < max; ++i)
     {	
      if(table[i].ppid < 1)
        continue;
#ifdef CS333_P4
      printf(1,"%d\t%s\t     %d\t        %d\t%d\t%d\t", table[i].pid, table[i].name,table[i].uid, table[i].gid, table[i].ppid, table[i].priority);

#else     
      printf(1,"%d\t%s\t     %d\t        %d\t%d\t", table[i].pid, table[i].name,table[i].uid, table[i].gid, table[i].ppid);
#endif
      int elapsed = table[i].elapsed_ticks / 1000;
      int elapsed_dec = table[i].elapsed_ticks % 1000;
      printf(1,"%d.", elapsed);
      if(elapsed_dec < 100 && elapsed_dec > 9)
        printf(1,"0%d\t", elapsed_dec);
      if(elapsed_dec < 10)
        printf(1,"00%d\t", elapsed_dec);
      if(elapsed_dec >=100)
        printf(1,"%d\t", elapsed_dec);
    // printf(1,"CPU TICKS: %d", table[i].CPU_total_ticks);
      int cpuelapsed = table[i].CPU_total_ticks /1000;
      int cpuelapsed_dec = table[i].CPU_total_ticks % 1000;    
      printf(1,"%d.", cpuelapsed);
      if(cpuelapsed_dec < 100 && cpuelapsed_dec > 9)
        printf(1,"0%d", cpuelapsed_dec);
      if(cpuelapsed_dec < 10)
        printf(1,"00%d", cpuelapsed_dec);
      if(cpuelapsed_dec >=100)
        printf(1,"%d", cpuelapsed_dec);
      printf(1, "   %s\t%d\n", table[i].state, table[i].size);
    } 
    
  free(table);
  exit();
}


#endif // CS333_P2

