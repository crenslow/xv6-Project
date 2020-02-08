// Some debug code for projects 3 and 4
// Verify that all processes exist on at least one state list.

// prototype
#ifdef DEBUG
static void checkProcs(const char *, const char *, int);
#endif

// example usage
#ifdef DEBUG
  checkProcs(__FILE__, __FUNCTION__, __LINE__);
#endif

// debug routines
#ifdef DEBUG
static int
procLookup(struct proc *p, struct proc *np)
{
  while (np != NULL) {
    if (np == p) return 1;
    np = np->next;
  }
  return 0;
}

static int
findProc(struct proc *p)
{
  if (procLookup(p, ptable.list[UNUSED].head)   != 0) return 1;
  if (procLookup(p, ptable.list[EMBRYO].head)   != 0) return 1;
  if (procLookup(p, ptable.list[RUNNING].head)  != 0) return 1;
  if (procLookup(p, ptable.list[SLEEPING].head) != 0) return 1;
  if (procLookup(p, ptable.list[ZOMBIE].head)   != 0) return 1;
  for (int i=0; i<=MAXPRIO; i++)
    if (procLookup(p, ptable.ready[i].head) != 0) return 1;

  return 0; // not found
}

static void
checkProcs(const char *file, const char *func, int line)
{
  int found;
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    found = findProc(p);
    if (found) continue;
    cprintf("checkprocs error. Called from %s, %s, @ %d\n", file, func, line);
    panic("Process array and lists inconsistent\n");
  }
}
#endif

