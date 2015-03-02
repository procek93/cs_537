#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "pstat.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  struct proc *ps;//pass search pointer to processes
  char *sp;

  //variable used to mark that first unused state has been seen
  int foundlock = 0;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack if possible.
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

  //set up parameters for stride scheduling
  //process gets mintix on default creation
  p->tickets = MINTIX;

  //set default stride value
  p->stride = (LCM/MINTIX);

  //***STARVATION PREVENTION***
  //on process creation, process will always take 
  //the smallest pass value among all existing processes
  //unless the process is the only one in existance.
  //**Iterate through the table, and for each unused state
  //**look at its pass value and see if smaller then last process
  acquire(&ptable.lock);
  for(ps = ptable.proc; ps < &ptable.proc[NPROC] ; ps++)
  {
    //only pay attention to used processes AND IGNORE THE PROCESS WE'RE COMPARING
    if((ps->state != UNUSED) && ((ps->pid) != (p->pid)))
    {
	//use simple lookahead by one algorithm to find min pass
	//always take first value on first iteration
	if(foundlock == 0)
	{
		//found lock is zero on start, once first used state
		//is seen, take its pass value as the basis for 
		//iterative comparisons
		p->pass = (ps->pass);
		foundlock = 1;
	}
	else
	if((ps->pass) < (p->pass))
	{
		//take newest smallest found pass value
		p->pass = (ps->pass);
	}
     }
  }  
  release(&ptable.lock);

  return p;
}

// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  
  p = allocproc();
  acquire(&ptable.lock);
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

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  
  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);
 
  pid = np->pid;
  np->state = RUNNABLE;
  safestrcpy(np->name, proc->name, sizeof(proc->name));
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  iput(proc->cwd);
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}


struct proc * get_lproc(void)
{
	struct proc * ps;
	struct proc * p = NULL;
	
	int foundlock = 0;
	
	for(ps = ptable.proc; ps < &ptable.proc[NPROC]; ps++){

		if(ps->state == RUNNABLE)
		{
			//found first runnable state, use this
			//as basis for comparison...Will also choose first
			//process found if all pass values are 0
			if(foundlock == 0)
			{
				p = ps;
				foundlock = 1;
			}
			else//otherwise search for runnable process with lowest pass
			if((ps->pass) < (p->pass))
			{
				p = ps;
			}
		}
	}

	return p;
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
 struct proc *p;
 struct proc *ps;

  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);

    p = get_lproc();

    if(p != NULL)
    {
      	//but first check for overflow
	//based on following calculation:
	//if limit is 5, and we have x + y, suppose x and y are 5
	//x + y will overflow...verification:
	// limit(5) - y(5) = 0:::: if x > (lim(5) - y(5))
	//aka, if 5 > 0, overflow happens
	if(((p->pass) > 0) && ((p->stride) > (LONG_MAX - (p->pass))))
	{
		//overflow is going to occur, zero out all pass values
		for(ps = ptable.proc; ps < &ptable.proc[NPROC]; ps++)
		{
			if(ps->state != UNUSED)
			{
				ps->pass = 0;
			}
		}
	}
	else//no overflow, increment pass
	{
		p->pass += p->stride;
	}

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      p->n_schedule++;

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);

  }
}

//Locks onto ptable and scans to find all non-unused 
//processes and returns number of processes found
int
num_procs(void)
{
	struct proc *p;
	int num_procs = 0;

	acquire(&ptable.lock);
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){

		//check for valid type
		if((p->state) > 5){
			release(&ptable.lock);
			return -1;
		}
		else if(p->state != UNUSED){
			num_procs++;
		}
	}

	release(&ptable.lock);

	return num_procs;
}
// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);
  
  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;
  struct proc *ps; //pass value search process pointer
  int foundlock = 0;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p->state == SLEEPING && p->chan == chan)
    {
      p->state = RUNNABLE;

      //***STARVATION PREVENTION***
      //on process wake, process will always take 
      //the smallest pass value among all existing processes
      //**Iterate through the table, and for each unused state
      //**look at its pass value and see if smaller then last process
      for(ps = ptable.proc; ps < &ptable.proc[NPROC] ; ps++)
      {
        //only pay attention to used processes and ignore the awoken process
        if((ps->state != UNUSED) && ((ps->pid) != (p->pid)))
        {
	   //use simple lookahead by one algorithm to find min pass
	   //always take first value on first iteration
	   if(foundlock == 0)
	   {
		 //found lock is zero on start, once first used state
		//is seen, take its pass value as the basis for 
		//iterative comparisons
		p->pass = (ps->pass);
		foundlock = 1;
	   }
	   else
	   if((ps->pass) < (p->pass))
	   {
	 	//take newest smallest found pass value
		p->pass = (ps->pass);
	   }
         }
      }  
    }
  }
}

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

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

//fills pstat table
int		
fill_pstat(void * pstat)
{
	struct proc *p;

	//create a pstat pointer
	struct pstat * procs;
	
	//variable to iterate procs pstat array
	int offset = 0;

	int x = 0;

	//cast user fed pointer
	procs = (struct pstat *)pstat;

	acquire(&ptable.lock);
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
	{
		//if the state is not used
		if(p->state == UNUSED)
		{
			//get the struct and fill out its variables
			(procs + offset)->inuse = 0;
			(procs + offset)->pid = 0;

			//because we're dealing with arrays and not pointers, we need to fill
			//the name using a manual for loop
			for(x = 0; x < 16; x++)
			{
				(procs + offset)->name[x] = (p->name[x]);
			}

			(procs + offset)->tickets = 0;
			(procs + offset)->pass = 0;
			(procs + offset)->stride = 0;
			(procs + offset)->n_schedule = 0;	
		}
		else//process slot in ptable is in use so fill info appropriately
		{
			//get the struct and fill out its variables
			(procs + offset)->inuse = 1;
			(procs + offset)->pid = (p->pid);

			//because we're dealing with arrays and not pointers, we need to fill
			//the name using a manual for loop
			for(x = 0; x < 16; x++)
			{
				(procs + offset)->name[x] = (p->name[x]);
			}

			(procs + offset)->tickets = (p->tickets);
			(procs + offset)->pass = (p->pass);
			(procs + offset)->stride = (p->stride);
			(procs + offset)->n_schedule = (p->n_schedule);
		}

		//next pstat entry
		offset++;
	}
	release(&ptable.lock);

	return 0;
}


