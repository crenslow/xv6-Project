#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#ifdef CS333_P2
#include "uproc.h"
#endif //CS333_P2
static char *states[] = {
[UNUSED]    "unused",
[EMBRYO]    "embryo",
[SLEEPING]  "sleep ",
[RUNNABLE]  "runble",
[RUNNING]   "run   ",
[ZOMBIE]    "zombie"
};

#ifdef CS333_P3
struct ptrs {
  struct proc* head;
  struct proc* tail;
};

#define statecount NELEM(states)
#endif //CS333_P3

static struct {
  struct spinlock lock;
  struct proc proc[NPROC];
#ifdef CS333_P3
  struct ptrs list[statecount];
#endif //CS333_P3
#ifdef CS333_P4
  struct ptrs ready[MAXPRIO+1];
  uint PromoteAtTime;
#endif //CS333_P4
} ptable;

// list management function prototypes
#ifdef CS333_P3
static void initProcessLists(void);
static void initFreeList(void);
static void stateListAdd(struct ptrs*, struct proc*);
static int  stateListRemove(struct ptrs*, struct proc* p);
static void assertState(struct proc *p, enum procstate state, const char * func, int line);

// list management helper functions
static void
stateListAdd(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL){
    (*list).head = p;
    (*list).tail = p;
    p->next = NULL;
  } else{
    ((*list).tail)->next = p;
    (*list).tail = ((*list).tail)->next;
    ((*list).tail)->next = NULL;
  }
}

static int
stateListRemove(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL || (*list).tail == NULL || p == NULL){
    return -1;
  }

  struct proc* current = (*list).head;
  struct proc* previous = 0;

  if(current == p){
    (*list).head = ((*list).head)->next;
    // prevent tail remaining assigned when we've removed the only item
    // on the list
    if((*list).tail == p){
      (*list).tail = NULL;
    }
    return 0;
  }

  while(current){
    if(current == p){
      break;
    }

    previous = current;
    current = current->next;
  }

  // Process not found. return error
  if(current == NULL){
    return -1;
  }

  // Process found.
  if(current == (*list).tail){
    (*list).tail = previous;
    ((*list).tail)->next = NULL;
  } else{
    previous->next = current->next;
  }

  // Make sure p->next doesn't point into the list.
  p->next = NULL;

  return 0;
}

static void
initProcessLists()
{
  int i;

  for (i = UNUSED; i <= ZOMBIE; i++) {
    ptable.list[i].head = NULL;
    ptable.list[i].tail = NULL;
  }
#ifdef CS333_P4
  for (i = 0; i <= MAXPRIO; i++) {
    ptable.ready[i].head = NULL;
    ptable.ready[i].tail = NULL;
  }
#endif
}

static void
initFreeList(void)
{
  struct proc* p;

  for(p = ptable.proc; p < ptable.proc + NPROC; ++p){
    p->state = UNUSED;
    stateListAdd(&ptable.list[UNUSED], p);
  }
}

// example usage:
// assertState(p, UNUSED, __FUNCTION__, __LINE__);
// This code uses gcc preprocessor directives. For details, see
// https://gcc.gnu.org/onlinedocs/cpp/Standard-Predefined-Macros.html
static void
assertState(struct proc *p, enum procstate state, const char * func, int line)
{
    if (p->state == state)
      return;
    cprintf("Error: proc state is %s and should be %s.\nCalled from %s line %d\n",
        states[p->state], states[state], func, line);
    panic("Error: Process state incorrect in assertState()");
}

#endif
static struct proc *initproc;

uint nextpid = 1;
extern void forkret(void);
extern void trapret(void);
static void wakeup1(void* chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;

  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid) {
      return &cpus[i];
    }
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
#ifdef CS333_P3
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  	
  if(ptable.list[UNUSED].head != NULL){
    p = ptable.list[UNUSED].head;
    if(stateListRemove(&ptable.list[p->state], p) < 0){
      panic("Process could not be removed from UNUSED list");
    }
    assertState(p, UNUSED, __FUNCTION__, __LINE__);
#ifdef CS333_P4
    p->priority = MAXPRIO;
    p->budget = DEFAULT_BUDGET;
#endif //CS333_P4
    p->state = EMBRYO;
    stateListAdd(&ptable.list[p->state],p);
  }
  else{
    release(&ptable.lock);
    return 0;
  }
 


  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    acquire(&ptable.lock);
    if(stateListRemove(&ptable.list[p->state], p) < 0){
      panic("Process could not be removed from EMBRYO list");
    }
    assertState(p, EMBRYO, __FUNCTION__, __LINE__);
    p->state = UNUSED;
    stateListAdd(&ptable.list[p->state], p);
    release(&ptable.lock);
   
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
#ifdef CS333_P1
  p->start_ticks = ticks;
#endif //CS333_P1
#ifdef CS333_P2
  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;
#endif //CS333_P2
  return p;
}
#else
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  int found = 0;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED) {
      found = 1;
      break;
    }
  if (!found) {
    release(&ptable.lock);
    return 0;
  }
  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
#ifdef CS333_P1
  p->start_ticks = ticks;
#endif //CS333_P1
#ifdef CS333_P2
  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;
#endif //CS333_P2
  return p;
}
#endif //CS333_P3
//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
#ifdef CS333_P4
  acquire(&ptable.lock);
  initProcessLists();
  initFreeList();
  ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
  release(&ptable.lock);
#endif
#ifdef CS333_P3
  acquire(&ptable.lock);
  initProcessLists();
  initFreeList();
  release(&ptable.lock);
#endif //CS333_P3
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S
#ifdef CS333_P2
  p->uid = UID;
  p->gid = GID;
#endif //CS333_P2
  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
#ifdef CS333_P4
  acquire(&ptable.lock);
  if(stateListRemove(&ptable.list[p->state],p) < 0){
    panic("Process could not be removed from the EMBRYO list");
  }
  assertState(p, EMBRYO, __FUNCTION__, __LINE__);
  p->state = RUNNABLE;
  stateListAdd(&ptable.ready[p->priority],p);
  release(&ptable.lock);
      

#elif CS333_P3
  acquire(&ptable.lock);
  if(stateListRemove(&ptable.list[p->state],p) < 0){
    panic("Process could not be removed from the EMBRYO list");
  }
  assertState(p, EMBRYO, __FUNCTION__, __LINE__);
  p->state = RUNNABLE;
  stateListAdd(&ptable.list[p->state],p);
  release(&ptable.lock);
      
#else
  acquire(&ptable.lock);
  p->state = RUNNABLE;
  release(&ptable.lock);
#endif //CS333_P3
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i;
  uint pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
#ifdef CS333_P3
    acquire(&ptable.lock);
    if(stateListRemove(&ptable.list[np->state],np) < 0){
      panic("Process could not be removed from EMBRYO list");
    }
    assertState(np, EMBRYO, __FUNCTION__, __LINE__);
    np->state = UNUSED;
    stateListAdd(&ptable.list[np->state], np);
    release(&ptable.lock);
#else
    np->state = UNUSED;
#endif //CS333_P3

    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;
#ifdef CS333_P2
  np->uid = curproc->uid;
  np->gid = curproc->gid;
#endif //CS333_P2
  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;
#ifdef CS333_P4
  acquire(&ptable.lock);
  if(stateListRemove(&ptable.list[np->state], np) < 0){
    panic("Process could not be removed from EMBRYO list");
  }

  assertState(np, EMBRYO, __FUNCTION__, __LINE__);
  np->state = RUNNABLE;

  stateListAdd(&ptable.ready[np->priority],np);

  release(&ptable.lock);
#elif CS333_P3
  acquire(&ptable.lock);
  if(stateListRemove(&ptable.list[np->state], np) < 0){
    panic("Process could not be removed from EMBRYO list");
  }

  assertState(np, EMBRYO, __FUNCTION__, __LINE__);
  np->state = RUNNABLE;
  stateListAdd(&ptable.list[np->state],np);

  release(&ptable.lock);
#else

  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);
#endif //CS333_P3

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
#ifdef CS333_P4
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.

  for(p= ptable.list[RUNNING].head; p!= NULL; p = p->next){
    if(p->parent == curproc){
      p->parent = initproc;
    }
  }
  for(int i = MAXPRIO; i >= 0; --i){
    for(p= ptable.ready[i].head; p!= NULL; p = p->next){
      if(p->parent == curproc){
        p->parent = initproc;
      }
    }
  }

  for(p= ptable.list[SLEEPING].head; p!= NULL; p = p->next){
    if(p->parent == curproc){
      p->parent = initproc;
    }
  }
  for(p= ptable.list[EMBRYO].head; p!= NULL; p = p->next){
    if(p->parent == curproc){
      p->parent = initproc;
    }
  }
  for(p = ptable.list[ZOMBIE].head; p != NULL; p = p->next){
    if(p->parent == curproc){
      p->parent = initproc;
      wakeup1(initproc);
    }
  }
  // Jump into the scheduler, never to return.
  if(stateListRemove(&ptable.list[curproc->state], curproc) < 0){
    panic("Process could not be removed from RUNNING list");
  }
  assertState(curproc, RUNNING, __FUNCTION__, __LINE__);
  curproc->state = ZOMBIE;
  stateListAdd(&ptable.list[curproc->state], curproc);
#ifdef PDX_XV6
  curproc->sz = 0;
#endif // PDX_XV6
  sched();
  panic("zombie exit");
}
#elif CS333_P3
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.

  for(p= ptable.list[RUNNING].head; p!= NULL; p = p->next){
    if(p->parent == curproc){
      p->parent = initproc;
    }
  }
  for(p= ptable.list[RUNNABLE].head; p!= NULL; p = p->next){
    if(p->parent == curproc){
      p->parent = initproc;
    }
  }

  for(p= ptable.list[SLEEPING].head; p!= NULL; p = p->next){
    if(p->parent == curproc){
      p->parent = initproc;
    }
  }
  for(p= ptable.list[EMBRYO].head; p!= NULL; p = p->next){
    if(p->parent == curproc){
      p->parent = initproc;
    }
  }
  for(p = ptable.list[ZOMBIE].head; p != NULL; p = p->next){
    if(p->parent == curproc){
      p->parent = initproc;
      wakeup1(initproc);
    }
  }
  // Jump into the scheduler, never to return.
  if(stateListRemove(&ptable.list[curproc->state], curproc) < 0){
    panic("Process could not be removed from RUNNING list");
  }
  assertState(curproc, RUNNING, __FUNCTION__, __LINE__);
  curproc->state = ZOMBIE;
  stateListAdd(&ptable.list[curproc->state], curproc);
#ifdef PDX_XV6
  curproc->sz = 0;
#endif // PDX_XV6
  sched();
  panic("zombie exit");
}
#else
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
#ifdef PDX_XV6
  curproc->sz = 0;
#endif // PDX_XV6
  sched();
  panic("zombie exit");
}
#endif //CS333_P3
// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
#ifdef CS333_P4
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.list[ZOMBIE].head; p != NULL; p = p->next){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
	if(stateListRemove(&ptable.list[p->state], p) < 0){
	  panic("Process could not be removed from ZOMBIE list");
	}
	assertState(p, ZOMBIE, __FUNCTION__, __LINE__);
        p->state = UNUSED;
	stateListAdd(&ptable.list[p->state], p);
       
        release(&ptable.lock);
        return pid;
      }
    }
    for(int i = MAXPRIO; i >= 0; --i){
      for(p = ptable.ready[i].head; p != NULL; p = p->next){
        if(p->parent == curproc){
          havekids = 1;
	  break;
        }	
      }
    }
    for(p = ptable.list[SLEEPING].head; p != NULL; p = p->next){
      if(p->parent == curproc){
        havekids = 1;
	break;
      }	
    }
    for(p = ptable.list[EMBRYO].head; p != NULL; p = p->next){
      if(p->parent == curproc){
        havekids = 1;
	break;
      }	
    }
    for(p = ptable.list[RUNNING].head; p != NULL; p = p->next){
      if(p->parent == curproc){
        havekids = 1;
	break;
      }	
    }
    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
#elif CS333_P3
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.list[ZOMBIE].head; p != NULL; p = p->next){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
	if(stateListRemove(&ptable.list[p->state], p) < 0){
	  panic("Process could not be removed from ZOMBIE list");
	}
	assertState(p, ZOMBIE, __FUNCTION__, __LINE__);
        p->state = UNUSED;
	stateListAdd(&ptable.list[p->state], p);
       
        release(&ptable.lock);
        return pid;
      }
    }
    for(p = ptable.list[RUNNABLE].head; p != NULL; p = p->next){
      if(p->parent == curproc){
        havekids = 1;
	break;
      }	
    }
    for(p = ptable.list[SLEEPING].head; p != NULL; p = p->next){
      if(p->parent == curproc){
        havekids = 1;
	break;
      }	
    }
    for(p = ptable.list[EMBRYO].head; p != NULL; p = p->next){
      if(p->parent == curproc){
        havekids = 1;
	break;
      }	
    }
    for(p = ptable.list[RUNNING].head; p != NULL; p = p->next){
      if(p->parent == curproc){
        havekids = 1;
	break;
      }	
    }
    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
#else
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
#endif //CS333_P3
//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
#ifdef CS333_P4
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(int i = MAXPRIO; i >= 0; --i){
      if(ptable.ready[i].head != NULL){
	p = ptable.ready[i].head; 


      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
#ifdef PDX_XV6
        idle = 0;  // not idle this timeslice
#endif // PDX_XV6



        c->proc = p;
        switchuvm(p);
	if(stateListRemove(&ptable.ready[p->priority],p) < 0){
          panic("Process could not be removed from the RUNNABLE list");
	}
	assertState(p, RUNNABLE, __FUNCTION__, __LINE__);
        p->state = RUNNING;
	stateListAdd(&ptable.list[p->state],p);
#ifdef CS333_P2
        p->cpu_ticks_in = ticks;
#endif //CS333_P2
        swtch(&(c->scheduler), p->context);


        switchkvm();
      // Process is done running for now.
      // It should have changed its p->state before coming back.
        c->proc = 0;
	break;
      }
    }
    if(ticks >= ptable.PromoteAtTime && MAXPRIO > 0){
      promote();
      ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
    }
    
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}
#elif CS333_P3
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.list[RUNNABLE].head; p != NULL; p = p->next){



      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
#ifdef PDX_XV6
        idle = 0;  // not idle this timeslice
#endif // PDX_XV6



        c->proc = p;
        switchuvm(p);
	if(stateListRemove(&ptable.list[p->state],p) < 0){
          panic("Process could not be removed from the RUNNABLE list");
	}
	assertState(p, RUNNABLE, __FUNCTION__, __LINE__);
        p->state = RUNNING;
	stateListAdd(&ptable.list[p->state],p);
#ifdef CS333_P2
        p->cpu_ticks_in = ticks;
#endif //CS333_P2
        swtch(&(c->scheduler), p->context);


        switchkvm();
      // Process is done running for now.
      // It should have changed its p->state before coming back.
        c->proc = 0;
	break;
    }   
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}
#else
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
#ifdef PDX_XV6
      idle = 0;  // not idle this timeslice
#endif // PDX_XV6



      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
#ifdef CS333_P2
      p->cpu_ticks_in = ticks;
#endif //CS333_P2
      swtch(&(c->scheduler), p->context);


      switchkvm();
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}
#endif //CS333_P3
// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();
 

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");

  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
#ifdef CS333_P2
  p->cpu_ticks_total += (ticks - p->cpu_ticks_in);
#endif //CS333_P2
#ifdef CS333_P4
  p->budget = p->budget - (ticks - p->cpu_ticks_in);
  /*  if(p->budget <= 0){
    if(p->priority != 0){
      if(stateListRemove(&ptable.ready[p->priority], p) <0){
        panic("Process could not be removed from the list");

      }	
      p->priority = p->priority - 1;
      assertState(p, RUNNABLE, __FUNCTION__, __LINE__);
      stateListAdd(&ptable.ready[p->priority], p);

    }

    p->budget = DEFAULT_BUDGET;
  }*/
  if(p->priority != 0 && p->budget <= 0)
    schedDemote(p->pid, p->priority -1);
#endif
  swtch(&p->context, mycpu()->scheduler);

  mycpu()->intena = intena;



}

// Give up the CPU for one scheduling round.
#ifdef CS333_P4
void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock
  if(stateListRemove(&ptable.list[curproc->state], curproc) <0){
    panic("Process could not be removed from the RUNNING list");	  
  }
  assertState(curproc, RUNNING, __FUNCTION__, __LINE__);
  curproc->state = RUNNABLE;
  curproc->budget = curproc->budget - (ticks - curproc->cpu_ticks_in);
  if(curproc->budget <= 0){
    if(curproc->priority != 0)
      curproc->priority = curproc->priority - 1;
    curproc->budget = DEFAULT_BUDGET;
  }
  stateListAdd(&ptable.ready[curproc->priority], curproc);
  sched();
  release(&ptable.lock);
}

#elif CS333_P3
void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock
  if(stateListRemove(&ptable.list[curproc->state], curproc) <0){
    panic("Process could not be removed from the RUNNING list");	  
  }
  assertState(curproc, RUNNING, __FUNCTION__, __LINE__);
  curproc->state = RUNNABLE;
  stateListAdd(&ptable.list[curproc->state], curproc);
  sched();
  release(&ptable.lock);
}
#else
void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock
  curproc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}
#endif //CS333_P3
// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
#ifdef CS333_P4
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  if(stateListRemove(&ptable.list[p->state],p) < 0){
    panic("Process could not be removed from the RUNNING list");
  }
  assertState(p, RUNNING, __FUNCTION__, __LINE__);
  p->budget = p->budget - (ticks - p->cpu_ticks_in);
  if(p->budget <= 0){
    if(p->priority != 0)
      p->priority = p->priority - 1;
    p->budget = DEFAULT_BUDGET;
  }
  p->state = SLEEPING;
  stateListAdd(&ptable.list[p->state], p);

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}

#elif CS333_P3
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  if(stateListRemove(&ptable.list[p->state],p) < 0){
    panic("Process could not be removed from the RUNNING list");
  }
  assertState(p, RUNNING, __FUNCTION__, __LINE__);
  p->state = SLEEPING;
  stateListAdd(&ptable.list[p->state], p);

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}
#else
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}

#endif //CS333_P3
//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
#ifdef CS333_P4
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.list[SLEEPING].head; p != NULL; p = p->next)
    if(p->chan == chan){
      if(stateListRemove(&ptable.list[p->state], p) < 0)
	panic("Process could not be removed from the list");
      assertState(p, SLEEPING, __FUNCTION__, __LINE__);
      p->state = RUNNABLE;
      stateListAdd(&ptable.ready[p->priority],p);
    }

}
#elif CS333_P3
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.list[SLEEPING].head; p != NULL; p = p->next)
    if(p->chan == chan){
      if(stateListRemove(&ptable.list[p->state], p) < 0)
	panic("Process could not be removed from the list");
      assertState(p, SLEEPING, __FUNCTION__, __LINE__);
      p->state = RUNNABLE;
      stateListAdd(&ptable.list[p->state],p);
    }

}
#else
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}
#endif //CS333_P3

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
#ifdef CS333_P4
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.list[SLEEPING].head; p != NULL; p = p->next){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING){
        if(stateListRemove(&ptable.list[p->state], p) < 0){
          panic("Process could not be removed from the SLEEPING list");
	}
	assertState(p, SLEEPING, __FUNCTION__, __LINE__);
        p->state = RUNNABLE;
	stateListAdd(&ptable.ready[p->priority], p);

      }
      release(&ptable.lock);
      return 0;
    }
  }
  for(p = ptable.list[RUNNING].head; p != NULL; p = p->next){
    if(p->pid == pid){
      p->killed = 1;
      
      release(&ptable.lock);
      return 0;
    }
  }
  for(int i = MAXPRIO; i >= 0; --i){
    for(p = ptable.ready[i].head; p != NULL; p = p->next){
      if(p->pid == pid){
        p->killed = 1;
      
        release(&ptable.lock);
        return 0;
     }
   }
  }

  for(p = ptable.list[EMBRYO].head; p != NULL; p = p->next){
    if(p->pid == pid){
      p->killed = 1;
      
      release(&ptable.lock);
      return 0;
    }
  }
  for(p = ptable.list[UNUSED].head; p != NULL; p = p->next){
    if(p->pid == pid){
      p->killed = 1;
      
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#elif CS333_P3
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.list[SLEEPING].head; p != NULL; p = p->next){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING){
        if(stateListRemove(&ptable.list[p->state], p) < 0){
          panic("Process could not be removed from the SLEEPING list");
	}
	assertState(p, SLEEPING, __FUNCTION__, __LINE__);
        p->state = RUNNABLE;
	stateListAdd(&ptable.list[p->state], p);

      }
      release(&ptable.lock);
      return 0;
    }
  }
  for(p = ptable.list[RUNNING].head; p != NULL; p = p->next){
    if(p->pid == pid){
      p->killed = 1;
      
      release(&ptable.lock);
      return 0;
    }
  }

  for(p = ptable.list[RUNNABLE].head; p != NULL; p = p->next){
    if(p->pid == pid){
      p->killed = 1;
      
      release(&ptable.lock);
      return 0;
    }
  }
  for(p = ptable.list[EMBRYO].head; p != NULL; p = p->next){
    if(p->pid == pid){
      p->killed = 1;
      
      release(&ptable.lock);
      return 0;
    }
  }
  for(p = ptable.list[UNUSED].head; p != NULL; p = p->next){
    if(p->pid == pid){
      p->killed = 1;
      
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#else
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#endif //CS333_P3
#ifdef CS333_P2
int
getprocs(int max, struct uproc * table)
{
  char * state;
  struct proc * p;
  int i = 0;

  acquire(&ptable.lock);

  for(p= ptable.proc; p < &ptable.proc[NPROC]&& i< max; p++){

     if( (p->state == UNUSED || p->state == EMBRYO))
       continue;	     
     table[i].pid = p->pid;
     table[i].uid = p->uid;
     table[i].gid = p->gid;
     if(p->parent == NULL)
       table[i].ppid = p->pid;
     else{
       table[i].ppid = p->parent->pid;
     }
#ifdef CS333_P4
     table[i].priority = p->priority; 
#endif
     table[i].elapsed_ticks = (ticks - p->start_ticks);

     table[i].CPU_total_ticks = p->cpu_ticks_total;
     state = states[p->state];
     safestrcpy(table[i].state, state, 32);
     table[i].size = p->sz;
     safestrcpy(table[i].name, p->name, sizeof(p->name));
     
     ++i;
    }
  release(&ptable.lock);
  return i;

}
#endif //CS333_P2
#ifdef CS333_P4
void
promote()
{
  struct proc* p;
  for(int i = MAXPRIO-1; i >= 0; --i){
    p = ptable.ready[i].head;
    while(p != NULL){
      struct proc* temp = p->next;

      if(stateListRemove(&ptable.ready[p->priority],p) < 0)
	panic("Could not remove from RUNNABLE");
      assertState(p, RUNNABLE, __FUNCTION__, __LINE__);
      p->priority = p->priority + 1;
      p->budget = DEFAULT_BUDGET;
      stateListAdd(&ptable.ready[p->priority],p);
      p = temp;
    }
  }
  for(p = ptable.list[RUNNING].head; p != NULL; p = p->next){
    if(p->priority != MAXPRIO){
       p->priority = p->priority + 1;
       p->budget = DEFAULT_BUDGET;
    }
  }
  for(p = ptable.list[SLEEPING].head; p != NULL; p = p->next){
    if(p->priority != MAXPRIO){
      p->priority = p->priority + 1;
      p->budget = DEFAULT_BUDGET;

    }
  }
}

int
getpriority(int pid)
{
  struct proc * p;
  int prio = -1;
  acquire(&ptable.lock);

  for(p = ptable.list[RUNNING].head; p != NULL; p = p->next){
    if(p->pid == pid){
      prio = p->priority;
      release(&ptable.lock);
      return prio;
    }
    else{
      break;
    }      
  }
  for(p = ptable.list[SLEEPING].head; p != NULL; p = p->next){
    if(p->pid == pid){
      prio = p->priority;
      release(&ptable.lock);
      return prio;
    }
    else{
      break;
    } 
  }
  for(p = ptable.list[EMBRYO].head; p != NULL; p = p->next){
    if(p->pid == pid){
      prio = p->priority;
      release(&ptable.lock);
      return prio;
    }
    else{
      break;
    } 
  }
  for(p = ptable.list[ZOMBIE].head; p != NULL; p = p->next){
    if(p->pid == pid){
      prio = p->priority;
      release(&ptable.lock);
      return prio;
    }
    else{
      break;
    } 
  }
  for(int i = MAXPRIO; i >= 0; --i){

    p = ptable.ready[i].head;
    while(p != NULL){
      struct proc* temp = p->next;

    //for(p = ptable.ready[i].head; p != NULL; p = p->next){
      if(p->pid == pid){
	prio = p->priority;
	release(&ptable.lock);
	return prio;
      }
      p = temp;
    }
  }
  release(&ptable.lock);  
  return prio;
}
//only used when ptable lock is being held 
struct proc*
FindPID(int pid)
{
  //acquire(&ptable.lock);
  struct proc * p;

  for(p = ptable.list[RUNNING].head; p != NULL; p = p->next){
    if(p->pid == pid){
//      release(&ptable.lock);
      return p;
    }
  }
  for(p = ptable.list[SLEEPING].head; p != NULL; p = p->next){
    if(p->pid == pid){
//      release(&ptable.lock);
      return p;
    }
  }
  for(int i = MAXPRIO; i >= 0; --i){
    p = ptable.ready[i].head;
    while(p != NULL){
      struct proc* temp = p->next;

    //for(p = ptable.ready[i].head; p != NULL; p = p->next){
      if(p->pid == pid){
//	release(&ptable.lock);
	return p;
      }
      p = temp;
    }
  }

  //release(&ptable.lock);
  return NULL;

}

int
setpriority(int pid, int priority)
{
  acquire(&ptable.lock);

  struct proc* p = FindPID(pid);
  if(p == NULL){
    release(&ptable.lock);
    return -1;
  }
  if(p->priority != priority && p->state == RUNNABLE){
    if(stateListRemove(&ptable.ready[p->priority],p) < 0)
      panic("Cannot remove from list");
    assertState(p, RUNNABLE, __FUNCTION__, __LINE__);
    p->budget = DEFAULT_BUDGET;
    p->priority = priority;
    stateListAdd(&ptable.ready[p->priority], p);
    release(&ptable.lock);
    return 0;
  }
  else{
    p->budget = DEFAULT_BUDGET;
    p->priority = priority;
    release(&ptable.lock);
    return 0;
  }
  release(&ptable.lock); 
  return -1;


}
//Called under sched only and while lock is held
void
schedDemote(int pid, int priority){
  
  struct proc * p = FindPID(pid);
  if(p == NULL){
    return ;
  }
  if(p->priority != priority && p->state == RUNNABLE){
    if(stateListRemove(&ptable.ready[p->priority],p) < 0)
      panic("Cannot remove from list");
    assertState(p, RUNNABLE, __FUNCTION__, __LINE__);
    p->budget = DEFAULT_BUDGET;
    p->priority = priority;
    stateListAdd(&ptable.ready[p->priority], p);
    return ;
  }
  else{
    p->budget = DEFAULT_BUDGET;
    p->priority = priority;
    return ;
  }
  return;
}
#endif
//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
#ifdef CS333_P1
void
procdumpP1(struct proc * p, char *state)
{
  int elapsed = (ticks - p->start_ticks) / 1000;
  int elapsed_dec = (ticks - p->start_ticks) % 1000;
  //cprintf("%d\t%s\t\t%d.%d\t%s\t%d\t", p->pid, p->name, elapsed,elapsed_dec, state, p->sz);
  cprintf("%d\t%s\t     %d.", p->pid, p->name, elapsed);
  if(elapsed_dec < 100 && elapsed_dec > 9)
    cprintf("0%d", elapsed_dec);
  if(elapsed_dec < 10)
    cprintf("00%d", elapsed_dec);
  if(elapsed_dec >=100)
    cprintf("%d", elapsed_dec);
  cprintf("\t%s\t%d\t", state, p->sz);	  

}
#endif //CS333_P1
#ifdef CS333_P2
 
void
procdumpP2(struct proc * p, char *state)
{
   cprintf("%d\t%s\t     %d\t        %d\t", p->pid, p->name,p->uid, p->gid);
   if(p->parent != NULL)
     cprintf("%d\t", p->parent->pid);
   else
     cprintf("%d\t", p->pid);

   int elapsed = (ticks - p->start_ticks) / 1000;
   int elapsed_dec = (ticks - p->start_ticks) % 1000;

   cprintf("%d.", elapsed);
   if(elapsed_dec < 100 && elapsed_dec > 9)
      cprintf("0%d\t", elapsed_dec);
   if(elapsed_dec < 10)
      cprintf("00%d\t", elapsed_dec);
   if(elapsed_dec >=100)
     cprintf("%d\t", elapsed_dec);
   int cpuelapsed = p->cpu_ticks_total /1000;
   int cpuelapsed_dec = p->cpu_ticks_total %1000;  
   cprintf("%d.", cpuelapsed);
   if(cpuelapsed_dec < 100 && cpuelapsed_dec > 9)
        cprintf("0%d", cpuelapsed_dec);
   if(cpuelapsed_dec < 10)
      cprintf("00%d", cpuelapsed_dec);
   if(cpuelapsed_dec >=100)
     cprintf("%d", cpuelapsed_dec);
   cprintf("   %s\t%d   ", state, p->sz);
     




}
#endif //CS333_P2
#ifdef CS333_P3
void
procdumpP3(struct proc * p, char * state)
{
   cprintf("%d\t%s\t     %d\t        %d\t", p->pid, p->name,p->uid, p->gid);
   if(p->parent != NULL)
     cprintf("%d\t", p->parent->pid);
   else
     cprintf("%d\t", p->pid);
#ifdef CS333_P4
   cprintf("%d\t", p->priority);
#endif
   int elapsed = (ticks - p->start_ticks) / 1000;
   int elapsed_dec = (ticks - p->start_ticks) % 1000;

   cprintf("%d.", elapsed);
   if(elapsed_dec < 100 && elapsed_dec > 9)
      cprintf("0%d\t", elapsed_dec);
   if(elapsed_dec < 10)
      cprintf("00%d\t", elapsed_dec);
   if(elapsed_dec >=100)
     cprintf("%d\t", elapsed_dec);
   int cpuelapsed = p->cpu_ticks_total /1000;
   int cpuelapsed_dec = p->cpu_ticks_total %1000;  
   cprintf("%d.", cpuelapsed);
   if(cpuelapsed_dec < 100 && cpuelapsed_dec > 9)
        cprintf("0%d", cpuelapsed_dec);
   if(cpuelapsed_dec < 10)
      cprintf("00%d", cpuelapsed_dec);
   if(cpuelapsed_dec >=100)
     cprintf("%d", cpuelapsed_dec);
   cprintf("   %s\t%d   ", state, p->sz);
}

#ifdef CS333_P4
void
ctrlr(void)
{
  acquire(&ptable.lock);
  struct proc * current;
  cprintf("Ready List Processes:\n");
  for(int i = MAXPRIO; i >= 0; --i){
    current = ptable.ready[i].head;
    cprintf("%d: ", i);    
    while(current != NULL){
      if(current->next == NULL)	  
        cprintf("(%d,%d)\n", current->pid, current->budget);
      else
        cprintf("(%d,%d)->", current->pid, current->budget);        
    
      current = current->next;

   }
   cprintf("\n");

  }
  release(&ptable.lock);
  cprintf("\n$ ");
}
#else
void
ctrlr(void)
{
  acquire(&ptable.lock);
  struct proc * current = ptable.list[RUNNABLE].head;
  cprintf("Ready List Processes:\n");
  while(current != NULL){
    if(current->next == NULL)	  
      cprintf("%d", current->pid);
    else
      cprintf("%d->", current->pid);
        
    
    current = current->next;

  }
  release(&ptable.lock);
  cprintf("\n$ ");
}
#endif
void
ctrlf(void)
{
  acquire(&ptable.lock);
  struct proc * current = ptable.list[UNUSED].head;
  int counted = 0;
  while(current != NULL){
    ++counted;  
    
    current = current->next;

  }
  cprintf("Free List Size: %d processes", counted);
  release(&ptable.lock);
  cprintf("\n$ ");
}
void
ctrls(void)
{
  acquire(&ptable.lock);
  struct proc * current = ptable.list[SLEEPING].head;
  cprintf("Sleeping List Processes:\n");
  while(current != NULL){
    if(current->next == NULL)	  
      cprintf("%d", current->pid);
    else
      cprintf("%d->", current->pid);   
    current = current->next;

  }
  release(&ptable.lock);
  cprintf("\n$ ");
}
void
ctrlz(void)
{
  acquire(&ptable.lock);
  struct proc * current = ptable.list[ZOMBIE].head;
  int ppid = 0;
  cprintf("Zombie List Processes:\n");
  while(current != NULL){
   if(current->parent != NULL)
     ppid = current->parent->pid;
   else
     ppid = current->pid;

   if(current->next == NULL)
     cprintf("(%d,%d)", current->pid, ppid);  
   else
     cprintf("(%d,%d)->", current->pid, ppid);  
    
    current = current->next;

  }
  release(&ptable.lock);
  cprintf("\n$ ");
}

#endif //CS333_P3
void
procdump(void)
{
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

#if defined(CS333_P4)
#define HEADER "\nPID\tName         UID\tGID\tPPID\tPrio\tElapsed\tCPU\tState\tSize\t PCs\n"
#elif defined(CS333_P3)
#define HEADER "\nPID\tName         UID\tGID\tPPID\tElapsed\tCPU\tState\tSize\t PCs\n"
#elif defined(CS333_P2)
#define HEADER "\nPID\tName         UID\tGID\tPPID\tElapsed\tCPU\tState\tSize\t PCs\n"
#elif defined(CS333_P1)
#define HEADER "\nPID\tName         Elapsed\tState\tSize\t PCs\n"
#else
#define HEADER "\n"
#endif

  cprintf(HEADER);  // not conditionally compiled as must work in all project states

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
 
#if defined(CS333_P3)
    procdumpP3(p, state);
#elif defined(CS333_P2)
    procdumpP2(p, state);
#elif defined(CS333_P1)
    procdumpP1(p, state);
#else
    cprintf("%d\t%s\t%s\t", p->pid, p->name, state);
#endif

    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
   }
 
#ifdef CS333_P1
  cprintf("$ ");  // simulate shell prompt
#endif // CS333_P1
}


